// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/GameInstance.h"
#include "OnlineSessionInterface.h"
#include "BSGameInstance.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class BATTLESTAGE_API UBSGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()
	
public:
	/** Set if this game instance is networked or local only. */
	UFUNCTION(BlueprintCallable, Category = GameInstance)
	void SetIsOnline(const bool IsOnline);

	/** Get if this game instance is networked or local only */
	UFUNCTION(BlueprintCallable, Category = GameInstance)
	bool GetIsOnline() const;

	UFUNCTION(BlueprintCallable, Category = GameInstance)
	bool HostSession(ULocalPlayer* LocalPlayer, const FString& TravelUrl);

	UFUNCTION(BlueprintCallable, Category = GameInstance)
	bool FindSessions(ULocalPlayer* LocalPlayer, const FString& Filters);

	void GracefullyDestroyOnlineSession();

	/** UGameInstance Interface Begin */
	virtual bool JoinSession(ULocalPlayer* LocalPlayer, const FOnlineSessionSearchResult& SearchResult) override;
	virtual bool JoinSession(ULocalPlayer* LocalPlayer, int32 SessionIndexInSearchResults) override;
	/** UGameInstance Interface End */

protected:
	void OnHostSessionCreated(FName SessionName, bool bWasSuccessful);

	void OnFindSessionsComplete(bool bWasSuccessful);

	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	void OnContinueDestroyingOnlineSession(FName SessionName, bool bWasSuccessful);

protected:
	/** Travel url held during async network operations */
	FString TravelURL;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameInstance)
	TSubclassOf<class UBSMatchConfig> MatchConfigClass;

private:
	class ABSGameSession* GetGameSession() const;

private:
	// If this game instance is networked or just local
	uint32 bIsOnline : 1;

	FDelegateHandle OnHostSessionCreatedHandle;
	FDelegateHandle OnFindSessionsCompleteHandle;
	FDelegateHandle OnJoinSessionCompleteHandle;
	FDelegateHandle OnContinueDestroyingOnlineSessionHandle;

	FOnCreateSessionCompleteDelegate OnContinueDestroyingOnlineSessionDelegate;

public:
	TSubclassOf<class UBSMatchConfig> GetMatchConfigClass() const { return MatchConfigClass; }
};
