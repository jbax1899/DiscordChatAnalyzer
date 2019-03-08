#pragma once
#ifndef GUILD_H_
#define GUILD_H_
struct guild
{
	string name;
	string id;
	bool selected;
	vector<channel> channels;
	guild(string inName, string inId)
	{
		name = inName;
		id = inId;
		selected = false;	//default: false
	}
};
#endif