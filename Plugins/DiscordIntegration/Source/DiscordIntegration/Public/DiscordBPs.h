// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Tickable.h"

// All this is needed otherwise this plugin will not compile
#define WIN32_LEAN_AND_MEAN

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include "../discord-files/dpp/dpp.h"
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "DiscordBPs.generated.h"

UENUM(BlueprintType)
enum class EDiscordReturnResult : uint8
{
	Ok = 0,
	ServiceUnavailable = 1,
	InvalidVersion = 2,
	LockFailed = 3,
	InternalError = 4,
	InvalidPayload = 5,
	InvalidCommand = 6,
	InvalidPermissions = 7,
	NotFetched = 8,
	NotFound = 9,
	Conflict = 10,
	InvalidSecret = 11,
	InvalidJoinSecret = 12,
	NoEligibleActivity = 13,
	InvalidInvite = 14,
	NotAuthenticated = 15,
	InvalidAccessToken = 16,
	ApplicationMismatch = 17,
	InvalidDataUrl = 18,
	InvalidBase64 = 19,
	NotFiltered = 20,
	LobbyFull = 21,
	InvalidLobbySecret = 22,
	InvalidFilename = 23,
	InvalidFileSize = 24,
	InvalidEntitlement = 25,
	NotInstalled = 26,
	NotRunning = 27,
	InsufficientBuffer = 28,
	PurchaseCanceled = 29,
	InvalidGuild = 30,
	InvalidEvent = 31,
	InvalidChannel = 32,
	InvalidOrigin = 33,
	RateLimited = 34,
	OAuth2Error = 35,
	SelectChannelTimeout = 36,
	GetGuildTimeout = 37,
	SelectVoiceForceRequired = 38,
	CaptureShortcutAlreadyListening = 39,
	UnauthorizedForAchievement = 40,
	InvalidGiftCode = 41,
	PurchaseError = 42,
	TransactionAborted = 43,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiscordResult, EDiscordReturnResult, StateSetResult);

UCLASS(NotBlueprintable, BlueprintType)
class DISCORDINTEGRATION_API UDiscordBPs : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bCanTick; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override { return TStatId(); }

private:
	static UDiscordBPs* DiscordBPsInstance;

	bool bCanTick = true;
	bool bTimerStarted;

	dpp::cluster* bot;
public:
	UDiscordBPs();

	/* Events */
	
	UPROPERTY(BlueprintAssignable, Category = "Discord|Delegates")
	FOnDiscordResult OnStateSet;

	UPROPERTY(BlueprintAssignable, Category = "Discord|Delegates")
	FOnDiscordResult OnDetailsSet;

	UPROPERTY(BlueprintAssignable, Category = "Discord|Delegates")
	FOnDiscordResult OnTimerStart;

	UPROPERTY(BlueprintAssignable, Category = "Discord|Delegates")
	FOnDiscordResult OnTimerEnd;

	UPROPERTY(BlueprintAssignable, Category = "Discord|Delegates")
	FOnDiscordResult OnJoinSecretSet;

	/* Functions */

	UFUNCTION(BlueprintCallable, Category = "Discord")
	static void CreateDiscordObject(FString InClientID, const bool bRequireDiscordRunning = false, const bool bStartElapsedTimer = true);

	UFUNCTION(BlueprintPure, Category = "Discord")
	static UDiscordBPs* GetDiscordObject();

	UFUNCTION(BlueprintCallable, Category = "Discord")
	void SetState(FString InNewState);

	UFUNCTION(BlueprintCallable, Category = "Discord")
	void SetDetails(FString InNewDetails);

	UFUNCTION(BlueprintCallable, Category = "Discord")
	void SetJoinSecret(const FString InNewJoinSecret);

	UFUNCTION(BlueprintCallable, Category = "Discord")
	void StartDiscordTimer();

	UFUNCTION(BlueprintCallable, Category = "Discord")
	void StopDiscordTimer();

	UFUNCTION(BlueprintPure, Category = "Discord")
	static const FString GetDiscordResultString(EDiscordReturnResult InDiscordResult);

	/* Discord Bot */
	UPROPERTY()
	int64 CurrentChannel = 927059555345629224; // Default is circuit-bot-testing channel

	UFUNCTION(BlueprintCallable, Category = "Discord|Message")
	void StartDiscordBot();

	UFUNCTION(BlueprintCallable, Category = "Discord|Message")
	void StopDiscordBot();

	UFUNCTION(BlueprintCallable, Category = "Discord|Message")
	void SendBotMessage(FString Message);
	
	UFUNCTION(BlueprintCallable, Category = "Discord|Message")
	void SendEmbedMessage(FString Message, FColor Color);
	
	UFUNCTION(BlueprintCallable, Category = "Discord|Message")
	void SetMessageChannel(int64 Channel);

private:
	void Internal_CreateDiscordObject(const FString& InClientID, const bool bRequireDiscordRunning, const bool bStartElapsedTimer);
};
