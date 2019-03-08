/*Discord Chat Analysis
Author: jbax1899
Purpose: Loads or downloads Discord chat logs, extracts data per user
Downloading is done using DiscordChatExporter created by Tyrrrz, see here:
https://github.com/Tyrrrz/DiscordChatExporter
My own repo:
https://github.com/jbax1899/DiscordChatAnalyzer/upload/master

To-do:
*	peopleGroups vector does not need to be 2-layer bucket sort. This system comes from an older, less efficient way of doing things.
*	more input validation
*	option for set download threads (currently has no limit)
*	!!more memory efficient (requires at least 4x the size of the input file)
*	kill analysis when out of memory
*	catch exception thrown when a user tries to download a channel they do not have access to. Looks messy as it is.
*	multithreaded analysis
*	GUI
*	graphical export
*	more analysis criteria
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <map>
#include <numeric>
#include <cstring>
#include <cstdio>
#include <windows.h>
#include <tchar.h>
#include <synchapi.h>
#include <thread>
#include <direct.h>
#include "Post.h"
#include "Person.h"
#include "Channel.h"
#include "Guild.h"

using namespace std;
using namespace std::chrono;

/*GLOBAL INITIALIZE*/
const int peopleGroupsSize = 100;		//[OLD!] 10k buckets, using ID, ex. #8173 = vector[81][73]
vector<vector<vector<Person>>> peopleGroupsEmpty(peopleGroupsSize);
string token = "";
int isBot = 0;
string addBot = "";
const short significantMostSaidWords = 100;
const short removeIfLongerThanChars = 20;
const short removeIfShorterThanChars = 4;
const string exporterLocation = "DiscordChatExporter.CLI\\DiscordChatExporter.CLi.exe ";	//using "\\" instead of escape char
int fileDownloadCount = 0;
vector<guild> guilds;
const short significantMostSaidWords_List = 5;
const short significantMostSaidWords_NumberLength = 8;
const short significantMostSaidWords_WordLength = 16;
const short threadCount = 1;	//multithreading

/*GLOBAL DYNAMIC*/
string fileName = "input.txt";		//will be changed a lot if downloading files
vector<string> myLines;
vector<string> swearWordsList;
//vector<vector<vector<Person>>> peopleGroups(peopleGroupsSize);
vector<vector<vector<Person>>> peopleGroups = peopleGroupsEmpty;
vector<string> userStrings;
size_t messageCount;
double profanityAverage;
vector<thread> threads(threadCount);
bool allGuilds = false;			//download everything?

bool Config();
void DownloadTask(string input);
void Download();
void Pull();
void Store();
void Analyse();
void Export();
bool fileExist(string fileNameIn);
bool CheckMemoryFree();

int main()
{		
	//START
	high_resolution_clock::time_point t1 = high_resolution_clock::now();	//execution time master
	std::cout << "------------------------------" << endl;
	std::cout << "Discord Data Analyser" << endl;
	std::cout << "Rolybug" << endl;
	std::cout << "------------------------------" << endl;
	
	//PULL CONFIG, RUN
	if (Config())
	{
		std::cout << "Loaded config." << endl;
		std::cout << "\n\n\n";
		std::cout << "Enter '1' if you have a valid text file to analyze, or '2' if you would like to download some." << endl;
		//SELECT MODE
		int option;
		for (;;)
		{
			if (std::cin >> option)
			{
				if (option == 1)
				{
					break;
				}
				else if (option == 2)
				{
					break;
				}
				else
				{
					std::cout << "Invalid option" << endl;
				}
			}
			else
			{
				std::cout << "Please enter a valid integer" << endl;
				std::cin.clear();
				std::cin.ignore(numeric_limits<streamsize>::max_digits10, '\n');
			}
		}
		if (option == 1)
		{
			high_resolution_clock::time_point t_analyze = high_resolution_clock::now();
			std::cout << "Analysing input file..." << endl;
			Pull();
			Store();
			Analyse();
			Export();
			//clear memory
			peopleGroups = peopleGroupsEmpty;
			//done
			high_resolution_clock::time_point t_analyze2 = high_resolution_clock::now();
			auto duration_analyze = duration_cast<microseconds>(t_analyze2 - t_analyze).count();
			std::cout << " done [" << (duration_analyze * 0.000001) << " seconds]" << endl;
		}
		else if (option == 2)
		{
			//Download Files
			Download();
			//Analyze loop
			for (int looper = 0; looper < fileDownloadCount; looper++)
			{
				string checkFileName = ("downloads/" + to_string(looper) + ".txt");
				if (fileExist(checkFileName))
				{
					fileName = checkFileName;
					std::cout << "Analysing " << (looper + 1) << " of " << fileDownloadCount << endl;
					//reset global variables first
					myLines.clear();
					swearWordsList.clear();
					peopleGroups = peopleGroupsEmpty;
					userStrings.clear();
					messageCount = 0;
					profanityAverage = 0;
					//run
					Pull();
					Store();
					Analyse();
					Export();
				}
			}
		}
		else
		{
			std::cout << "Something went VERY wrong." << endl;
		}
	}
	else
	{
		cerr << "Failed to load required file(s)!" << endl;
	}
	//Done here!
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(t2 - t1).count();
	std::cout << endl << "Program finished! Execution time: " << (duration * 0.000001) << " seconds" << endl;
	//ALL DONE
	std::cin.get();
	std::cin.get();
	return 0;
}

bool Config()
{
	//config file
	clog << "Reading configuration file..." << endl;
	std::ifstream configFile("settings.cfg");
	for (std::string lineRunner; getline(configFile, lineRunner); )
	{
		if (lineRunner.substr(0, 2) != "//")			//ignore comments altogether
		{
			if (lineRunner.substr(0, 6) == "token=") { token = lineRunner.substr(6, 60); }
			if (lineRunner.substr(0, 6) == "bot=") { isBot = stoi(lineRunner.substr(4, 1)); }
		}
	}
	configFile.close();
	messageCount = myLines.size();
	if (token == "")
	{
		cerr << "No token supplied! Please add this to the config file and try again." << endl;
		return false;
	}
	else if (token.length() != 59)
	{
		cerr << "Token provided is invalid (wrong size). Please check this again and try again." << endl;
		return false;
	}
	//DiscordChatExporter check
	std::ifstream checkFile("DiscordChatExporter.CLI\\DiscordChatExporter.CLi.exe");
	if (!checkFile.good())
	{
		cerr << "Error - DiscordChatExporter was not found at the proper location, DiscordChatExporter.CLI\\DiscordChatExporter.Cli.exe" << endl;
		return false;
	}
	//All good!
	return true;
}

void DownloadTask(string input)
{
	char tempChar[200];
	strcpy_s(tempChar, input.c_str());
	try
	{
		system(tempChar);
	}
	catch (...)	//does not work
	{
		std::cout << "Download failed due to lack of permissions." << endl;
		std::cout << "(" << input << ")" << endl;
	}
}

void Download()
{
	/*
		1. Ask for token
		2. run DiscordChatExporter once and save guilds to .txt
		3. store guild IDs to memory
		4. run DiscordChatExporter for every guild, save channels to .txt
		5. store channels IDs to memory
		6. run DiscordChatExporter for every channel, save chatlog to .txt
		7. continue program (for each chatlog downloaded)
	*/
	_mkdir("temp");
	if (isBot == 1)
		addBot = " -b";		//need to add flag if this is a bot
	else if (isBot == 0)
		addBot = "";
	string getGuilds = (exporterLocation + "guilds -t " + token + addBot + " > temp/guilds.txt");
	char getGuildsChar[200];	//big enough, probably
	strcpy_s(getGuildsChar, getGuilds.c_str());
	system(getGuildsChar);		//run guild gather and store
	//Get Guilds
	//List
	std::ifstream guildsFile("temp/guilds.txt");
	std::cout << endl << "Available Guilds: " << endl;
	int guildsCounter = 0;
	for (std::string lineRunner; getline(guildsFile, lineRunner); )
	{
		std::cout << "[" << guildsCounter << "] " << lineRunner << endl;
		guilds.emplace_back(guild(lineRunner.substr(21, lineRunner.length()), lineRunner.substr(0, 18)));
		guildsCounter++;
	}
	guildsFile.close();
	//Which Guild(s)?
	std::cout << endl << "Which guilds would you like to download from? Separate with commas (ex. 1,5,32) or 'a' for all (and all channels)" << endl;
	string guildInput;
	std::cin >> guildInput;
	if (guildInput == "a")
	{
		allGuilds = true;
		for (size_t i = 0; i < guilds.size(); i++)
		{
			guilds[i].selected = true;
		}
		clog << guilds.size() << " guilds selected" << endl;
	}
	else
	{
		vector<int> selectedGuildsTemp;
		string tempGuildsBuilder = "";
		if (guildInput[guildInput.length()] != ',') { guildInput = guildInput + ','; }	//for conversion to int vector
		for (size_t i = 0; i < guildInput.length(); i++)	//pull selected guilds
		{
			if (guildInput[i] == ',')
			{
				selectedGuildsTemp.push_back(stoi(tempGuildsBuilder));
				tempGuildsBuilder = "";
			}
			else
			{
				tempGuildsBuilder += guildInput[i];
			}
		}
		for (size_t i = 0; i < selectedGuildsTemp.size(); i++)
		{
			guilds[selectedGuildsTemp[i]].selected = true;
			clog << "Guild " << selectedGuildsTemp[i] << " selected" << endl;
		}
	}
	//Get Channels
	for (size_t ch = 0; ch < guilds.size(); ch++)	//build channels file
	{
		if (guilds[ch].selected == true)
		{
			ofstream channelsOutFile("temp/[" + to_string(ch) + "]channels.txt");	//create blank
			channelsOutFile.close();
			string getChannels = (exporterLocation + "channels -t "
				+ token + addBot + " -g " + guilds[ch].id + " >> temp/[" + to_string(ch)
				+ "]channels.txt");								//>> is APPENDING, not overwriting!
			char getChannelsChar[200];
			strcpy_s(getChannelsChar, getChannels.c_str());
			system(getChannelsChar);							//run script
		}
	}
	int channelsCounter = 0;
	for (size_t ch = 0; ch < guilds.size(); ch++)	//read from channel text files
	{
		if (guilds[ch].selected == true)
		{
			if (allGuilds == true)
			{
				int tempTotal = 0;
				for (size_t i = 0; i < guilds[ch].channels.size(); i++)
				{
					guilds[ch].channels[i].selected = true;
					tempTotal++;
				}
				std::cout << tempTotal << " channels selected" << endl;
			}
			else
			{
				//List
				std::cout << endl << "[" << ch << "] Channels: " << endl;
				std::cout << guilds[ch].name << ":" << endl;
				std::ifstream channelsFile("temp/[" + to_string(ch) + "]channels.txt");
				for (std::string lineRunner; getline(channelsFile, lineRunner); )
				{
					channelsCounter++;
					std::cout << "[" << (channelsCounter - 1) << "]" << lineRunner << endl;
					guilds[ch].channels.emplace_back(
						lineRunner.substr(21, lineRunner.length()),
						lineRunner.substr(0, 18),
						guilds[ch].id);
				}
				channelsFile.close();

				//Which Channel(s)?
				std::cout << endl << "Which channels? Separate with commas (ex. 1,5,32), or 'a' for all" << endl;
				string channelInput;
				std::cin >> channelInput;

				if (channelInput == "a")
				{
					for (size_t i = 0; i < guilds[ch].channels.size(); i++)
					{
						guilds[ch].channels[i].selected = true;
					}
					clog << guilds[ch].channels.size() << " channels selected" << endl;
				}
				else
				{
					vector<int> selectedChannelsTemp;
					string tempChannelsBuilder = "";
					if (channelInput[channelInput.length()] != ',') { channelInput = channelInput + ','; }	//for conversion to int vector
					for (size_t i = 0; i < channelInput.length(); i++)	//pull selected guilds
					{
						if (channelInput[i] == ',')
						{
							selectedChannelsTemp.push_back(stoi(tempChannelsBuilder));
							tempChannelsBuilder = "";
						}
						else
						{
							tempChannelsBuilder += channelInput[i];
						}
					}
					for (size_t i = 0; i < selectedChannelsTemp.size(); i++)
					{
						guilds[ch].channels[i].selected = true;
						clog << "Channel " << selectedChannelsTemp[i] << " selected" << endl;
					}
				}
			}
		}
	}
	//Build download strings
	vector<string> outputDownloads;		//temp download strings
	for (size_t ch = 0; ch < guilds.size(); ch++)
	{
		for (size_t chx = 0; chx < guilds[ch].channels.size(); chx++)	//selected guilds, selected channels
		{
			if (guilds[ch].channels[chx].selected == true)
			{
				outputDownloads.push_back(exporterLocation + "export -t "
					+ token + addBot + " -c "
					+ guilds[ch].channels[chx].id + " -f PlainText -o downloads/"
					+ to_string(fileDownloadCount) + ".txt");
				fileDownloadCount++;
			}
		}
	}
	//Execute download strings
	for (size_t dl = 0; dl < outputDownloads.size(); dl++)
	{
		threads.emplace_back(thread(DownloadTask, outputDownloads[dl]));
	}
	for (std::thread & th : threads)
	{
		if (th.joinable())
			th.join();
	}
}

void Pull()
{
	//chatlog lines
	std::ifstream chatlog(fileName);
	/*
	//get the number of messages listed at top of file
	string messageCountTemp = "";
	for (int i = 0; i < 5; ++i)
		std::getline(myFile, messageCountTemp);
	messageCountTemp.replace(0, 10, "");
	for (int i = 0; i < 10; ++i)
	{
		std::size_t found = messageCountTemp.find(",");
		if (found != string::npos)
		{
			messageCountTemp.replace(messageCountTemp.find(","), 1, "");
		}
	}
	long messageCount = stol(messageCountTemp);
	*/
	for (std::string lineRunner; getline(chatlog, lineRunner); )
		if (lineRunner != "") { myLines.emplace_back(lineRunner); }
	chatlog.close();
	messageCount = myLines.size();
	//swear words
	//technically don't have to do this each time but whatever
	std::ifstream swearFile("swearWords.txt");
	for (std::string lineRunner; getline(swearFile, lineRunner); )
		swearWordsList.emplace_back(lineRunner);
	swearFile.close();
}

void Store()
{
	string tempName = "nan";
	string dayTemp = "nan";
	string monthTemp = "nan";
	string yearTemp = "nan";
	string dateTemp = "nan";
	string timeTemp = "nan";
	string messageTemp = "nan";
	string tempBucket = "nan";
	string tempID = "9999";
	short tempID_A = 9999;
	short tempID_B = 9999;
	short bucketIndex = -1;
	short timestampCounter = -1;
	string line;
	double progress = 0.0;
	//create database
	for (int i = 0; i < peopleGroupsSize; i++)
	{
		for (int ii = 0; ii < peopleGroupsSize; ii++)
		{
			peopleGroups[i].emplace_back(vector<Person>());
		}
	}
	vector<vector<vector<Person>>> *peopleGroupsPtr;
	peopleGroupsPtr = &peopleGroups;
	//line loop
	for (size_t linesLoop = 0; linesLoop < messageCount; linesLoop++)
	{
		line = myLines[linesLoop];
		/*
		if ((linesLoop % (messageCount / 4) == 0) && (linesLoop != 0)) //progress display
		{
			progress += 0.25;
			int barWidth = 100;
			std::std::cout << "[";
			double pos = barWidth * progress;
			for (int i = 0; i < barWidth; ++i)
			{
				if (i < pos) std::std::cout << "=";
				else if (i == pos) std::std::cout << ">";
				else std::std::cout << " ";
			}
			std::std::cout << "] " << int(progress * 100.0) << " %\r" << endl;
		}
		*/
		if (line != "")
		{
			if ((line.length() > 20) && (line.substr(0, 1) == "[") && (line.substr(18, 2) == "M]"))
				//is our line a timestamp formatted: [07-Jul-17 05:23 AM] squarebit#9235
			{
				dayTemp = line.substr(1, 2);
				monthTemp = line.substr(4, 3);
				yearTemp = line.substr(8, 2);
				dateTemp = line.substr(1, 9);
				timeTemp = line.substr(11, 2) + line.substr(14, 2);
				tempName = line.substr(21, line.length() - 21);
				tempID = tempName.substr(tempName.length() - 4, 4);
				timestampCounter = 0;
			}
			else
			{
				//nope, regular message
				if (timestampCounter != -1)
					timestampCounter++;
			}
			//FIND USER
			tempID_A = stoi(tempID.substr(0, 2));	//what bucketA?
			tempID_B = stoi(tempID.substr(2, 2));	//what bucketB?
			bucketIndex = -1;						//if name is found in database this will be overwritten
			for (unsigned int f = 0; f < peopleGroups[tempID_A][tempID_B].size(); f++)	//is he in there?
			{
				if (peopleGroups[tempID_A][tempID_B][f].name == tempName)
				{
					bucketIndex = f;
				}
			}
			if ((timestampCounter == 0) && (bucketIndex == -1))	//if timestamp and a new person, add them to the list
			{
				peopleGroups[tempID_A][tempID_B].emplace_back(Person(tempName, dateTemp));
			}
			else if (timestampCounter > 0)						//if not a timestamp, add to last user
			{
				peopleGroups[tempID_A][tempID_B][bucketIndex].posts.emplace_back(Post(dayTemp, monthTemp, yearTemp, timeTemp, line));
				peopleGroups[tempID_A][tempID_B][bucketIndex].lastPost = dateTemp;
			}
		}
		//myLines[linesLoop] = "";
	}
	//SAVES MEMORY
	vector<string>().swap(myLines);
}

void Analyse()
{
	typedef map<string, int> word_count_list;	//most-said words, count
	double progress2 = 0;
	//start loop
	for (size_t i = 0; i < peopleGroupsSize; i++)
	{
		for (size_t ii = 0; ii < peopleGroupsSize; ii++)
		{
			for (size_t p = 0; p < peopleGroups[i][ii].size(); p++)
			{
				/*
				if ((p % (peopleGroups[i][ii].size() / 4) == 0) && (p != 0)) //progress display
				{
					progress2 += 0.25;
					int barWidth = 100;
					std::std::cout << "[";
					double pos = barWidth * progress2;
					for (int i = 0; i < barWidth; ++i)
					{
						if (i < pos) std::std::cout << "=";
						else if (i == pos) std::std::cout << ">";
						else std::std::cout << " ";
					}
					std::std::cout << "] " << int(progress2 * 100.0) << " %\r" << endl;
				}
				*/
				word_count_list word_count;							//most-said words map
				word_count_list profane_count;
				std::vector<std::pair<string, int>> word_sorting;	//vector from map, for sorting and resize
				std::vector<std::pair<string, int>> profaneWords;
				for (size_t po = 0; po < peopleGroups[i][ii][p].posts.size(); po++)
				{
					//SAME WORDS
					istringstream iss(peopleGroups[i][ii][p].posts[po].message);
					vector<string> results(istream_iterator<string>{iss}, istream_iterator<string>());
					for (size_t calcx = 0; calcx < results.size(); calcx++)
					{
						if ((results[calcx].length() > removeIfShorterThanChars) && (results[calcx].length() < removeIfLongerThanChars))	//stuff like links and "I, a, or" are bad.
						{
							size_t index;
							while ((index = results[calcx].find_first_of(".,!?\\;-*+'")) != string::npos)	//remove punctuation
							{
								results[calcx].erase(index, 1);
							}
							std::transform(results[calcx].begin(), results[calcx].end(), results[calcx].begin(), ::tolower); //to lowercase
							++word_count[results[calcx]];
						}
						//profanity
						if (std::find(swearWordsList.begin(), swearWordsList.end(), results[calcx]) != swearWordsList.end())	//is it in our swear words file?
						{
							++profane_count[results[calcx]];
						}
					}
				}
				//ADD TO VECTOR, SORT		
				//https://stackoverflow.com/questions/5056645/sorting-stdmap-using-value
				//most-said words
				for (auto itr = word_count.begin(); itr != word_count.end(); ++itr)
				{
					word_sorting.push_back(*itr);
				}
				sort(word_sorting.begin(), word_sorting.end(), [=](std::pair<string, int>& a, std::pair<string, int>& b)
				{
					return b.second < a.second;
				});
				//profane words
				for (auto itr = profane_count.begin(); itr != profane_count.end(); ++itr)
				{
					profaneWords.push_back(*itr);
				}
				sort(profaneWords.begin(), profaneWords.end(), [=](std::pair<string, int>& a, std::pair<string, int>& b)
				{
					return b.second < a.second;
				});
				//profanity
				double tempWordCount = 0;
				for (size_t pc = 0; pc < word_sorting.size(); pc++)		//need faster way of getting the sum of seconds in pairs
				{
					tempWordCount += word_sorting[pc].second;
				}
				double tempProfanityCount = 0;
				for (size_t pc = 0; pc < profaneWords.size(); pc++)
				{
					tempProfanityCount += profaneWords[pc].second;
				}
				if ((tempWordCount > 0) && (tempProfanityCount > 0))
				{
					peopleGroups[i][ii][p].profaneAverage = (tempProfanityCount / tempWordCount);
				}
				//REMOVE TOO-FEW SAID WORDS
				word_sorting.resize(significantMostSaidWords);
				//ADD TO PERSON DATA
				peopleGroups[i][ii][p].mostSaidWords = word_sorting;
				peopleGroups[i][ii][p].profanityWords = profaneWords;
				//CLEAR MEMORY
				word_count.clear();
				profane_count.clear();
				vector<pair<string, int>>().swap(word_sorting);
				vector<pair<string, int>>().swap(profaneWords);
			}
		}
	}
	//profanity
	double tempPeopleCount = 0;
	double tempProfanity = 0;
	for (size_t i = 0; i < peopleGroupsSize; i++)
	{
		for (size_t ii = 0; ii < peopleGroupsSize; ii++)
		{
			for (size_t p = 0; p < peopleGroups[i][ii].size(); p++)
			{
				tempPeopleCount++;
				tempProfanity += peopleGroups[i][ii][p].profaneAverage;
			}
		}
	}
	profanityAverage = ((1 / tempPeopleCount) * tempProfanity);
}

void Export()
{
	long int totalPeople = 0;
	long int totalPosts = 0;
	for (size_t i = 0; i < peopleGroupsSize; i++)
	{
		for (size_t ii = 0; ii < peopleGroupsSize; ii++)
		{
			for (size_t p = 0; p < peopleGroups[i][ii].size(); p++)
			{
				string usdTempName = peopleGroups[i][ii][p].name;
				usdTempName.erase(std::remove(usdTempName.begin(), usdTempName.end(), ','), usdTempName.end()); //remove commas
				totalPeople++;
				for (size_t po = 0; po < peopleGroups[i][ii][p].posts.size(); po++)
				{
					totalPosts++;
				}
				if (peopleGroups[i][ii][p].posts.size() > 0)	//filter out the zeroes
				{
					//most-said words
					string mostSaidWordsBuilder = "\""; //as we are separating values with commas, we must put this whole string in a field like: "value(10),value(5),value(2)"
					for (size_t msw = 0; msw < significantMostSaidWords_List; msw++)
					{
						string tempNumber = ("|[" + to_string(peopleGroups[i][ii][p].mostSaidWords[msw].second) + "] ");
						string tempWord = (peopleGroups[i][ii][p].mostSaidWords[msw].first + "  ");
						tempNumber.resize(significantMostSaidWords_NumberLength, ' ');
						tempWord.resize(significantMostSaidWords_WordLength, ' ');
						mostSaidWordsBuilder += (tempNumber + tempWord);
					}
					mostSaidWordsBuilder += "\"";	//end of CSV field
					//this is what we're gonna print out
					userStrings.emplace_back(
						usdTempName.substr(0, usdTempName.length() - 5)
						+ "," + usdTempName.substr(usdTempName.length() - 5, usdTempName.length())
						+ "," + to_string(peopleGroups[i][ii][p].posts.size())
						+ "," + peopleGroups[i][ii][p].firstPost
						+ "," + peopleGroups[i][ii][p].lastPost
						+ "," + to_string(peopleGroups[i][ii][p].profaneAverage * 100).substr(0, 4) + "%"
						+ "," + mostSaidWordsBuilder
						+ "\n"	//end of line
					);
				}
			}
		}
	}
	//PRINT TO CSV
	_mkdir("exports");
	if (fileName == "input.txt")
	{
		std::ofstream data_csv("exports/input_data.csv");
		data_csv << "People (" << totalPeople << "),ID,Posts (" << totalPosts
			<< "),First Post,Last Post,Profanity per Word (average " << to_string((profanityAverage * 100)).substr(0, 4) << "%),Most-Said Words (" << removeIfShorterThanChars
			<< "-" << removeIfLongerThanChars << " chars)"
			<< endl;
		for (size_t uds = 0; uds < userStrings.size(); uds++)
		{
			data_csv << userStrings[uds];
		}
		data_csv.close();
	}
	else
	{
		std::ofstream data_csv("exports/" + fileName.substr(10, fileName.length()) + "_data.csv"); //remove "downloads/" part	//have to use "\\"
		data_csv << "People (" << totalPeople << "),ID,Posts (" << totalPosts
			<< "),First Post,Last Post,Profanity per Word (average " << to_string((profanityAverage * 100)).substr(0, 4) << "%),Most-Said Words (" << removeIfShorterThanChars
			<< "-" << removeIfLongerThanChars << " chars)"
			<< endl;
		for (size_t uds = 0; uds < userStrings.size(); uds++)
		{
			data_csv << userStrings[uds];
		}
		data_csv.close();
	}
	//CLEAR MEMORY
	vector<string>().swap(userStrings);
}

bool fileExist(string fileNameIn)
{
	std::ifstream inFile(fileNameIn);
	if (!inFile.good())
		std::cout << fileNameIn << " not found" << endl;
	return inFile.good();
}

bool CheckMemoryFree()
{
	//https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);
	DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile;
	DWORDLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;
	double threshold = 0.95;
	if (virtualMemUsed / totalVirtualMem > threshold)
	{
		return false;
	}
	return true;
}