// Fill out your copyright notice in the Description page of Project Settings.

#include "BattleStage.h"
#include "BSGameSession.h"
#include "Online.h"
#include "Online/BSOnlineSessionSettings.h"
#include "../OnlineSubsystem/Public/Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"


ABSGameSession::ABSGameSession(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &ABSGameSession::OnCreateDelegateComplete);
	OnStartOnlineSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &ABSGameSession::OnStartOnlineSessionComplete);
	OnFindSessionsCompletedDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &ABSGameSession::OnFindSessionsComplete);
	OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &ABSGameSession::OnJoinSessionComplete);
}

bool ABSGameSession::CreateSession(ULocalPlayer* LocalPlayer, const FName SessionName, const int32 MaxConnections, const bool bIsLan, const FString& GameType, const FString& MapName)
{
	UE_LOG(BattleStageOnline, Log, TEXT("Creating online session %s for %s"), *SessionName.ToString(), *LocalPlayer->GetName());

	UWorld* const World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
	if (SessionPtr.IsValid())
	{
		FBSOnlineSessionSettings Settings(MaxConnections, bIsLan, MapName, GameType);

		OnCreateSessionCompleteHandle = SessionPtr->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
		return SessionPtr->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), SessionName, Settings);
	}
	else
	{
		UE_LOG(BattleStageOnline, Warning, TEXT("ABSGameSession::CreateSession could not find a valid online session interface."));
		return false;
	}
}

void ABSGameSession::OnCreateDelegateComplete(FName SessionName, bool bWasSuccessful)
{
	UWorld* const World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
	if (SessionPtr.IsValid())
	{
		SessionPtr->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteHandle);

		if (bWasSuccessful)
		{
			UE_LOG(BattleStageOnline, Log, TEXT("Online session successfully created."));
			OnStartOnlineSessionHandle = SessionPtr->AddOnStartSessionCompleteDelegate_Handle(OnStartOnlineSessionCompleteDelegate);
			SessionPtr->StartSession(SessionName);
		}
		else
		{
			UE_LOG(BattleStageOnline, Log, TEXT("Online session could not be created."));
		}
	}
}

void ABSGameSession::OnStartOnlineSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UWorld* const World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
	if (SessionPtr.IsValid())
	{
		SessionPtr->ClearOnStartSessionCompleteDelegate_Handle(OnStartOnlineSessionHandle);
	}

	OnSessionCreated().Broadcast(SessionName, bWasSuccessful);
}

bool ABSGameSession::FindSessions(ULocalPlayer* LocalPlayer)
{
	UE_LOG(BattleStageOnline, Log, TEXT("Starting to find online sessions for %s"), *LocalPlayer->GetName());

	UWorld* const World = GetWorld();
	check(World);

	bool bOperationSuccessful = false;

	IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
	if (SessionPtr.IsValid())
	{
		OnFindSessionsCompleteHandle = SessionPtr->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompletedDelegate);

		SearchSettings = MakeShareable(new FBSOnlineSessionSearch());
		bOperationSuccessful = SessionPtr->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SearchSettings.ToSharedRef());
	}

	return bOperationSuccessful;
}

void ABSGameSession::OnFindSessionsComplete(bool bWasSuccessful)
{	
	UE_LOG(BattleStageOnline, Log, TEXT("Find sessions completed: %s, found %d sessions."), bWasSuccessful ? TEXT("successfully") : TEXT("failure"), bWasSuccessful ? SearchSettings->SearchResults.Num() : 0);

	UWorld* const World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
	if (SessionPtr.IsValid())
	{
		SessionPtr->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteHandle);
	}

	OnFindSessionsComplete().Broadcast(bWasSuccessful && SearchSettings->SearchResults.Num() > 0);
}

const TArray<FOnlineSessionSearchResult>& ABSGameSession::GetSearchResults() const
{
	static const TArray<FOnlineSessionSearchResult> EmptySearch;
	return SearchSettings.IsValid() ? SearchSettings->SearchResults : EmptySearch;
}

bool ABSGameSession::JoinSession(ULocalPlayer* LocalPlayer, const FOnlineSessionSearchResult& SearchResult)
{
	bool bOperationSuccessful = false;

	if (SearchResult.IsValid())
	{
		UE_LOG(BattleStageOnline, Log, TEXT("Starting to join online session by %s for %s"), *SearchResult.Session.OwningUserName, *LocalPlayer->GetName());

		UWorld* const World = GetWorld();
		check(World);
		
		IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
		if (SessionPtr.IsValid())
		{
			OnJoinSessionCompleteHandle = SessionPtr->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);

			SessionPtr->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), GameSessionName, SearchResult);
		}
	}
	else
	{
		UE_LOG(BattleStageOnline, Warning, TEXT("%s attempted to join online session using invalid search result."), *LocalPlayer->GetName());
	}

	return bOperationSuccessful;
}

void ABSGameSession::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(BattleStageOnline, Log, TEXT("Join session completed %s"), Result == EOnJoinSessionCompleteResult::Success ? TEXT("successfully") : TEXT("unsuccessfully"));
	
	UWorld* const World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionPtr = Online::GetSessionInterface(World);
	if (SessionPtr.IsValid())
	{
		SessionPtr->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteHandle);
	}

	OnJoinSessionComplete().Broadcast(SessionName, Result);
}
