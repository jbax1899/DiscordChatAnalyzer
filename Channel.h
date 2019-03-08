#pragma once
#ifndef CHANNEL_H_
#define CHANNEL_H_
struct channel
{
	string name, id, guildId;
	bool selected;
	channel(string inName, string inId, string inGuild)
	{
		name = inName;
		id = inId;
		guildId = inGuild;
		selected = false;	//default: false
	}
};
#endif