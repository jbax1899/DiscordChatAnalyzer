#pragma once
#ifndef POST_H_
#define POST_H_
#include <string>
using namespace std;
struct Post
{
	string day, year, month, time, message;	//year ex. 1999, military time ex. 23:46 (not 11:46pm)
	Post(string dayIn, string monthIn, string yearIn, string timeIn, string messageIn)
	{
		day = dayIn;
		month = monthIn;
		year = yearIn;
		time = timeIn;
		message = messageIn;
	}
};
#endif