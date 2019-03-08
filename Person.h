#pragma once
#ifndef PERSON_H_
#define PERSON_H_
struct Person
{
	string name, firstPost, lastPost;
	double profaneAverage;
	vector<Post> posts;
	vector<pair<string, int>> mostSaidWords;
	vector<pair<string, int>> profanityWords;
	vector<pair<string, int>> controversialWords;
	Person(string inputName, string inputFirstPost)
	{
		name = inputName;
		firstPost = inputFirstPost;
	}
};
#endif