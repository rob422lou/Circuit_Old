// Fill out your copyright notice in the Description page of Project Settings.

#include "DiscordBPs.h"
#include "../discord-files/discord.h"

DEFINE_LOG_CATEGORY_STATIC(LogDiscord, Log, All)

#define LogDisplay(Param1)		UE_LOG(LogDiscord, Display, TEXT("%s"), *FString(Param1))
#define LogError(Param1)		UE_LOG(LogDiscord, Error, TEXT("%s"), *FString(Param1))

discord::Core* core{};
discord::Activity activity{};

UDiscordBPs* UDiscordBPs::DiscordBPsInstance = nullptr;

UDiscordBPs::UDiscordBPs()
{
	bCanTick = bTimerStarted = false;
}

void UDiscordBPs::Tick(float DeltaTime)
{
	if (bCanTick)
	{
		::core->RunCallbacks();
	}
}

void UDiscordBPs::CreateDiscordObject(FString InClientID, const bool bRequireDiscordRunning /*= false*/, const bool bStartElapsedTimer /*= true*/)
{
	if (DiscordBPsInstance == nullptr)
	{
		DiscordBPsInstance = NewObject<UDiscordBPs>();
		DiscordBPsInstance->AddToRoot();
		DiscordBPsInstance->Internal_CreateDiscordObject(InClientID, bRequireDiscordRunning, bStartElapsedTimer);
	}
}

void UDiscordBPs::Internal_CreateDiscordObject(const FString& InClientID, const bool bRequireDiscordRunning, const bool bStartElapsedTimer)
{
#if WITH_EDITOR
	discord::Result result = discord::Core::Create(FCString::Atoi64(*InClientID), DiscordCreateFlags_NoRequireDiscord, &core);
#else
	discord::Result result = discord::Core::Create(FCString::Atoi64(*InClientID), bRequireDiscordRunning ? DiscordCreateFlags_Default : DiscordCreateFlags_NoRequireDiscord, &core);
#endif
	if (result == discord::Result::Ok)
	{
		DiscordBPsInstance->bCanTick = true;
		LogDisplay("Discord object created.");

		if (bStartElapsedTimer)
		{
			DiscordBPsInstance->StartDiscordTimer();
			core->SetLogHook(discord::LogLevel::Debug, [](discord::LogLevel level, const char* message)
				{
					//LogDisplay(FString::Printf(TEXT("SetLogHook: %s"), *FString(message)));
				});
		
		}
	}
}

UDiscordBPs* UDiscordBPs::GetDiscordObject()
{
	return DiscordBPsInstance;
}

void UDiscordBPs::SetState(FString InNewState)
{
	activity.SetState(TCHAR_TO_UTF8(*InNewState));
	if (core)
	{
		core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
			{
				uint8 ResultByte = (uint8)result;
				DiscordBPsInstance->OnStateSet.Broadcast(static_cast<EDiscordReturnResult>(ResultByte));
				LogDisplay(FString::Printf(TEXT("State Set Result: %s"), *GetDiscordResultString(static_cast<EDiscordReturnResult>(ResultByte))));
			});
	}
}

void UDiscordBPs::SetDetails(FString InNewDetails)
{
	activity.SetDetails(TCHAR_TO_UTF8(*InNewDetails));
	if (core)
	{
		core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
			{
				uint8 ResultByte = (uint8)result;
				DiscordBPsInstance->OnDetailsSet.Broadcast(static_cast<EDiscordReturnResult>(ResultByte));
				LogDisplay(FString::Printf(TEXT("Details Set Result: %s"), *GetDiscordResultString(static_cast<EDiscordReturnResult>(ResultByte))));
			});
	}
}

void UDiscordBPs::SetJoinSecret(FString InNewJoinSecret)
{
	activity.GetSecrets().SetJoin(TCHAR_TO_UTF8(*InNewJoinSecret));
	if (core)
	{
		core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
			{
				uint8 ResultByte = (uint8)result;
				DiscordBPsInstance->OnJoinSecretSet.Broadcast(static_cast<EDiscordReturnResult>(ResultByte));
				LogDisplay(FString::Printf(TEXT("Join secret set. Result: %s"), *GetDiscordResultString(static_cast<EDiscordReturnResult>(ResultByte))));
			});
	}
}

void UDiscordBPs::StartDiscordTimer()
{
	// probably only needed in the editor, but this resets the time across multiple sessions, preventing 00:00 remaining from displaying.
	activity.GetTimestamps().SetEnd(0);

	activity.GetTimestamps().SetStart(FDateTime::UtcNow().ToUnixTimestamp());
	if (core)
	{
		core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
			{
				uint8 ResultByte = (uint8)result;
				DiscordBPsInstance->OnTimerStart.Broadcast(static_cast<EDiscordReturnResult>(ResultByte));
				LogDisplay(FString::Printf(TEXT("Timer Start Result: %s"), *GetDiscordResultString(static_cast<EDiscordReturnResult>(ResultByte))));
			});
	}
}

void UDiscordBPs::StopDiscordTimer()
{
	DiscordBPsInstance->bTimerStarted = false;
	activity.GetTimestamps().SetEnd(FDateTime::UtcNow().ToUnixTimestamp());
	if (core)
	{
		core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
			{
				uint8 ResultByte = (uint8)result;
				DiscordBPsInstance->OnTimerEnd.Broadcast(static_cast<EDiscordReturnResult>(ResultByte));
				LogDisplay(FString::Printf(TEXT("Timer End Result: %s"), *GetDiscordResultString(static_cast<EDiscordReturnResult>(ResultByte))));
			});
	}
}

const FString UDiscordBPs::GetDiscordResultString(EDiscordReturnResult InDiscordResult)
{
	return UEnum::GetDisplayValueAsText(InDiscordResult).ToString();
}

void UDiscordBPs::StartDiscordBot()
{
	bot = new dpp::cluster("OTI2OTU3OTMzNDU4MjMxMzY2.YdDO2Q.yQcSv1uAUkpNzeY3tu1ydV18oXQ");

	bot->start();
}

void UDiscordBPs::StopDiscordBot() {
	if (!bot) {
		return;
	}
	delete bot;
}

void UDiscordBPs::SendBotMessage(FString Message)
{
	if (!bot) {
		return;
	}
	bot->message_create(dpp::message(CurrentChannel, TCHAR_TO_UTF8(*Message), dpp::message_type::mt_default));
}

// @FIXME - This spits out the wrong color.
void UDiscordBPs::SendEmbedMessage(FString Message, FColor Color) {
	if (!bot) {
		return;
	}
	//LogDisplay(FString::Printf(TEXT("Color: %s %i"), *Color.ToHex(), FParse::HexNumber(*Color.ToHex())));
	dpp::embed embed = dpp::embed().set_color(FParse::HexNumber(*Color.ToHex()))
		.set_title(TCHAR_TO_UTF8(*Message))
		.set_url("")
		.set_author("", "", "")
		.set_description("")
		.set_thumbnail("");

	bot->message_create(dpp::message(CurrentChannel, embed));
}

void UDiscordBPs::SetMessageChannel(int64 Channel) {
	CurrentChannel = Channel;
}