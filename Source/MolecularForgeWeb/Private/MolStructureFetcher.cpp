// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolStructureFetcher.h"
#include "MolStructureCache.h"
#include "MolecularStructure.h"
#include "MolStructureIO.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
// Basisklasse des Abrufkontexts weiter unten.
#include "UObject/GCObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
	/**
	 * Kennzeichnet unsere Anfragen gegenueber RCSB und EMBL-EBI.
	 * Beide betreiben oeffentliche Dienste ohne Schluessel und ohne Anmeldung; sich dabei
	 * erkennbar zu machen ist das Mindeste. Wer Probleme verursacht, soll ansprechbar sein.
	 */
	const TCHAR* GUserAgent = TEXT("MolecularForge/0.1 (Unreal Engine Plugin; +https://github.com/SimulatedFlow)");

	/**
	 * Haelt einen laufenden Abruf zusammen.
	 *
	 * Erbt von FGCObject, weil die Struktur zwischen ihrer Erzeugung und dem Abschluss
	 * des Hintergrundtasks von nichts anderem referenziert wird — ohne das koennte die
	 * Speicherbereinigung sie einsammeln, waehrend der Parser noch hineinschreibt.
	 *
	 * Wird auf dem Spielthread erzeugt und dort auch wieder freigegeben: der letzte
	 * Verweis liegt immer in der Fortsetzung, die auf dem Spielthread laeuft.
	 */
	class FMolFetchContext : public FGCObject
	{
	public:
		FMolFetchOptions Options;
		FOnMolStructureFetched OnComplete;
		TWeakObjectPtr<UObject> Outer;

		TObjectPtr<UMolecularStructure> Structure;
		FString ResolvedUrl;
		double StartTime = 0.0;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Structure);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("MolecularForge.FetchContext");
		}
	};

	using FMolFetchContextPtr = TSharedPtr<FMolFetchContext, ESPMode::ThreadSafe>;

	void CompleteWithError(const FMolFetchContextPtr& Context, const FString& Error, int32 StatusCode = 0)
	{
		check(IsInGameThread());

		FMolFetchResult Result;
		Result.bSuccess = false;
		Result.Error = Error;
		Result.HttpStatusCode = StatusCode;
		Result.ResolvedUrl = Context->ResolvedUrl;
		Result.ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - Context->StartTime);

		UE_LOG(LogMolecularForge, Warning, TEXT("Abruf fehlgeschlagen (%s %s): %s"),
			*MolecularForge::GetSourceSlug(Context->Options.Source), *Context->Options.Identifier, *Error);

		Context->OnComplete.ExecuteIfBound(nullptr, Result);
	}

	/**
	 * Zerlegt den Dateiinhalt im Hintergrund und meldet sich danach auf dem Spielthread.
	 * Die Struktur wird noch hier auf dem Spielthread erzeugt — UObjects anzulegen ist
	 * anderswo nicht erlaubt. Befuellt wird sie dann im Hintergrund, was zulaessig ist,
	 * weil dabei nur ihre Arrays angefasst werden.
	 */
	void ParseAndComplete(const FMolFetchContextPtr& Context, FString&& Content, bool bFromCache)
	{
		check(IsInGameThread());

		UObject* Owner = Context->Outer.IsValid() ? Context->Outer.Get() : GetTransientPackage();
		Context->Structure = NewObject<UMolecularStructure>(Owner);

		const FMolLoadOptions LoadOptions = Context->Options.LoadOptions;
		TSharedPtr<FString> Payload = MakeShared<FString>(MoveTemp(Content));

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Context, Payload, LoadOptions, bFromCache]()
		{
			FMolParseResult ParseResult =
				MolecularForge::ParseStructureText(*Payload, LoadOptions, *Context->Structure);

			AsyncTask(ENamedThreads::GameThread, [Context, ParseResult, bFromCache]()
			{
				FMolFetchResult Result;
				Result.bSuccess = ParseResult.bSuccess;
				Result.Error = ParseResult.Error;
				Result.bFromCache = bFromCache;
				Result.ResolvedUrl = Context->ResolvedUrl;
				Result.HttpStatusCode = bFromCache ? 0 : 200;
				Result.ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - Context->StartTime);

				if (!ParseResult.bSuccess)
				{
					UE_LOG(LogMolecularForge, Warning, TEXT("Abgerufene Datei nicht lesbar: %s"), *ParseResult.Error);
					Context->OnComplete.ExecuteIfBound(nullptr, Result);
					return;
				}

				// Die Herkunft an der Struktur vermerken, damit die Attribution stimmt,
				// auch wenn die Datei selbst nichts darueber sagt.
				UMolecularStructure* Structure = Context->Structure;
				if (Context->Options.Source == EMolFetchSource::AlphaFoldDb)
				{
					Structure->Meta.Source = EMolStructureSource::AlphaFoldDb;
					Structure->Meta.Attribution = MolecularForge::GetAlphaFoldAttribution();
				}
				else if (Structure->Meta.Source != EMolStructureSource::AlphaFoldDb)
				{
					Structure->Meta.Source = EMolStructureSource::RcsbPdb;
					Structure->Meta.Attribution = MolecularForge::GetPdbAttribution();
				}

				if (Structure->Meta.Identifier.IsEmpty())
				{
					Structure->Meta.Identifier = Context->Options.Identifier;
				}

				UE_LOG(LogMolecularForge, Log, TEXT("Struktur bezogen (%s, %.2f s): %s"),
					bFromCache ? TEXT("Cache") : TEXT("Netz"), Result.ElapsedSeconds, *Structure->GetSummary());

				Context->OnComplete.ExecuteIfBound(Structure, Result);
			});
		});
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Url, float TimeoutSeconds)
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("User-Agent"), GUserAgent);
		Request->SetHeader(TEXT("Accept"), TEXT("*/*"));
		Request->SetTimeout(TimeoutSeconds);
		return Request;
	}

	/** Zweiter Schritt bei AlphaFold und einziger Schritt bei RCSB: die Datei selbst holen. */
	void DownloadFile(const FMolFetchContextPtr& Context, const FString& FileUrl)
	{
		Context->ResolvedUrl = FileUrl;

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
			MakeRequest(FileUrl, Context->Options.TimeoutSeconds);

		Request->OnProcessRequestComplete().BindLambda(
			[Context](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				CompleteWithError(Context, TEXT("Keine Verbindung zum Server."));
				return;
			}

			const int32 StatusCode = Response->GetResponseCode();
			if (StatusCode == 404)
			{
				CompleteWithError(Context, FString::Printf(
					TEXT("'%s' ist bei dieser Quelle nicht vorhanden."), *Context->Options.Identifier), StatusCode);
				return;
			}
			if (StatusCode < 200 || StatusCode >= 300)
			{
				CompleteWithError(Context, FString::Printf(
					TEXT("Der Server antwortete mit Status %d."), StatusCode), StatusCode);
				return;
			}

			FString Content = Response->GetContentAsString();
			if (Content.IsEmpty())
			{
				CompleteWithError(Context, TEXT("Der Server lieferte eine leere Antwort."), StatusCode);
				return;
			}

			// Erst in den Cache, dann zerlegen. Umgekehrt bliebe bei einem Parserfehler
			// nichts liegen und der naechste Versuch wuerde erneut herunterladen.
			if (Context->Options.bUseCache)
			{
				MolecularForge::WriteCachedStructure(
					Context->Options.Source, Context->Options.Identifier, Content);
			}

			ParseAndComplete(Context, MoveTemp(Content), /*bFromCache=*/false);
		});

		Request->ProcessRequest();
	}

	/** Erster Schritt bei AlphaFold: die API nach dem Eintrag fragen. */
	void ResolveAlphaFoldEntry(const FMolFetchContextPtr& Context, const FString& ApiUrl)
	{
		Context->ResolvedUrl = ApiUrl;

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
			MakeRequest(ApiUrl, Context->Options.TimeoutSeconds);

		Request->OnProcessRequestComplete().BindLambda(
			[Context](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				CompleteWithError(Context, TEXT("Keine Verbindung zur AlphaFold-Datenbank."));
				return;
			}

			const int32 StatusCode = Response->GetResponseCode();
			if (StatusCode == 404)
			{
				CompleteWithError(Context, FString::Printf(
					TEXT("Fuer '%s' gibt es keine AlphaFold-Vorhersage."), *Context->Options.Identifier), StatusCode);
				return;
			}
			if (StatusCode < 200 || StatusCode >= 300)
			{
				CompleteWithError(Context, FString::Printf(
					TEXT("Die AlphaFold-API antwortete mit Status %d."), StatusCode), StatusCode);
				return;
			}

			// Die Antwort ist ein JSON-Array mit einem Eintrag je Modellfragment.
			TArray<TSharedPtr<FJsonValue>> Entries;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

			if (!FJsonSerializer::Deserialize(Reader, Entries) || Entries.IsEmpty())
			{
				CompleteWithError(Context, TEXT("Die Antwort der AlphaFold-API war nicht lesbar."), StatusCode);
				return;
			}

			const TSharedPtr<FJsonObject> Entry = Entries[0]->AsObject();
			if (!Entry.IsValid())
			{
				CompleteWithError(Context, TEXT("Die Antwort der AlphaFold-API hatte nicht die erwartete Form."), StatusCode);
				return;
			}

			// mmCIF bevorzugt: es bringt die Konfidenzangaben in maschinenlesbarer Form mit.
			FString FileUrl;
			if (!Entry->TryGetStringField(TEXT("cifUrl"), FileUrl) || FileUrl.IsEmpty())
			{
				if (!Entry->TryGetStringField(TEXT("pdbUrl"), FileUrl) || FileUrl.IsEmpty())
				{
					CompleteWithError(Context, TEXT("Der AlphaFold-Eintrag nennt keine Datei-Adresse."), StatusCode);
					return;
				}
			}

			DownloadFile(Context, FileUrl);
		});

		Request->ProcessRequest();
	}
}

namespace MolecularForge
{
	void FetchStructure(UObject* Outer, const FMolFetchOptions& Options, FOnMolStructureFetched OnComplete)
	{
		if (!IsInGameThread())
		{
			UE_LOG(LogMolecularForge, Error, TEXT("FetchStructure muss vom Spielthread aus aufgerufen werden."));
			FMolFetchResult Result;
			Result.Error = TEXT("Aufruf nicht vom Spielthread.");
			OnComplete.ExecuteIfBound(nullptr, Result);
			return;
		}

		FMolFetchContextPtr Context = MakeShared<FMolFetchContext, ESPMode::ThreadSafe>();
		Context->Options = Options;
		Context->Options.Identifier = NormalizeIdentifier(Options.Source, Options.Identifier);
		Context->OnComplete = OnComplete;
		Context->Outer = Outer;
		Context->StartTime = FPlatformTime::Seconds();

		if (Context->Options.Identifier.IsEmpty())
		{
			CompleteWithError(Context, TEXT("Es wurde keine Kennung angegeben."));
			return;
		}

		if (!IsValidIdentifier(Context->Options.Source, Context->Options.Identifier))
		{
			const FString Expectation = Context->Options.Source == EMolFetchSource::RcsbPdb
				? TEXT("Erwartet wird ein PDB-Code wie '1CRN'.")
				: TEXT("Erwartet wird eine UniProt-Accession wie 'P69905'.");

			CompleteWithError(Context, FString::Printf(
				TEXT("'%s' ist keine gueltige Kennung. %s"), *Context->Options.Identifier, *Expectation));
			return;
		}

		// Der Cache kommt vor allem anderen. Siehe Begruendung in MolStructureCache.h.
		if (Context->Options.bUseCache)
		{
			FString Cached;
			if (ReadCachedStructure(Context->Options.Source, Context->Options.Identifier,
				Cached, Context->Options.CacheMaxAgeDays))
			{
				ParseAndComplete(Context, MoveTemp(Cached), /*bFromCache=*/true);
				return;
			}
		}

		const FString Url = BuildRequestUrl(Context->Options.Source, Context->Options.Identifier);
		if (Url.IsEmpty())
		{
			CompleteWithError(Context, TEXT("Es liess sich keine Adresse fuer diese Anfrage bilden."));
			return;
		}

		switch (Context->Options.Source)
		{
		case EMolFetchSource::AlphaFoldDb:
			ResolveAlphaFoldEntry(Context, Url);
			break;

		case EMolFetchSource::RcsbPdb:
		default:
			DownloadFile(Context, Url);
			break;
		}
	}
}
