/*
Author: Atanas B. Golev
Instructor: Dr. Finkel
Date: 4/3/2017
Assignment: A new shell
Description: A basic shell that allows users to execute a number of built-in and program-control commands.
*/

#include <vector>
#include <string.h> //strmp, strtok
#include <sstream>
#include <iostream>
#include <stdlib.h> //malloc, realloc, free, exit, execvp
#include <stdio.h>  //fprintf, printf, stderr, getchar, perror
#include <unistd.h> //chdir(), fork(), exec(), pid_t
#include <sys/wait.h> //waitpid()
#include <signal.h>

using namespace std;

//Used to determine whether user command is valid
string validCommands[8] = {"set", "prompt", "dir", "procs", "done", "do", "back", "tovar"}; 

vector<string> backJobs;//List of all background jobs

vector<string> allVariables;	//variables names set by the user
vector<string> allValues;	//values associated with variable names set by the user

//list of the tokens in an input command, along with int acting as boolean for whether command is valid
struct tokenList{
	vector<string> tokens;	//list of tokens
	int isGood;		//1 if valid; 0 if not
};

/*
Input: name of variable
Output: values associated with that variables
*/
string indexSearch(string query){
	for(int i=0; i<allVariables.size(); i++)	//Searches list of all variables
		if(allVariables[i] ==query)
			return allValues[i];		//Returns associated values
	cerr<<"Variable not found!";			//Should never get here because it's checked with isFound, but just in case
	return "NULL";
}

//Input: name of variable; Output: 1 if there is a variable with such name, 0 otherwise
bool isFound(string query){
	for(int i=0; i<allVariables.size(); i++){
		if(allVariables[i] == query)
			return true;
		}
	return false;
}

/*
Scans an input line and separates it into tokens. There's a special condition if the command
is a prompt because we don't want to lose any blank spaces. In that case, just seperates
the command into one token for "prompt" and another token for the rest of the string.
*/
vector<string> scanner(string line){
	vector<string> newTokenList;	//the new list we will return
	string temp;
	int locateComment = 0;		//used for prompt special case in finding % comments

	//Special case if "Prompt"
	if(line.size()>=6 && line.substr(0,6)=="prompt"){
		newTokenList.push_back("prompt");	//Sets first variable to prompt
		locateComment = line.find('%');		//Makes sure comments don't interfere
		if(line.size()<=8)			//Makes sure they are setting the prompt to somethign; if not returns list which is read as not valid by isValid function
			return newTokenList;

		if(locateComment==-1)		//Used to identify position of %
			newTokenList.push_back(line.substr(7));
		else
			newTokenList.push_back(line.substr(7,locateComment-7));

		return newTokenList;
	}

	//In all other cases, tokens are split by spaces, and line stops whenever it hits %
	stringstream splitter(line);
	while(splitter >> temp && temp!="%"){
		newTokenList.push_back(temp);
		}
	return newTokenList;
}

//Scans a line to determine whether it is a valid user command
bool isValid(vector<string> test){

	bool isValidCommand = false;

	if (test.size()==0)	//A blank line is a valid command that doesn't do anythind
		return true;

	else if(test[0]=="prompt" && test.size()==1){//If user commands prompt, but does not provide prompt
		cerr<<"You did not include a prompt command!"<<endl;
		return false;
		}

	//The procs and done commands only takes the one argument; nothing else
	else if((test[0]=="procs" || test[0]=="done") && test.size()!=1){
		cerr<<"Wrong number of arguments! "<<test.size() <<endl;
		return false;
		}

	//The dir command takes exactly two arguments
	else if(test[0]=="dir" && test.size()!=2){
		cerr<<"Wrong number of arguments!"<<endl;
		return false;
		}

	//The set command takes exactly three arguments
	else if(test[0]=="set" && test.size()!=3){
		cerr<<"Wrong number of arguments!"<<endl;
		return false;
		}

	//The tovar command takes no less that 4 arguments
	else if(test[0]=="tovar" && test.size()<4){
		cerr<<"Wrong number of arguments!"<<endl;
		return false;
		}

	//The do and back commands take no less than three arguments
	else if((test[0]=="do" || test[0]=="back")&&test.size()<2){
		cerr<<"Wrong number of arguments!"<<endl;
		return false;
		}

	//This loop makes sure that the first word in the user input line is a valid command
	for(int i=0; i<=7; i=i+1){
		if(validCommands[i]==test[0])
			isValidCommand = true;
	}

	//Error message if command is not valid
	if(!isValidCommand)
		cerr<<"You did not enter a valid command! Please try again"<<endl;

	return isValidCommand;
}

//Returns new TokenList based on user input line
//This includes all tokens, and also a variable for whether the input line is a valid one
//This is a second check for accuracy, in case the scanner function didn't pick up an error
tokenList parser(string userInput){
	tokenList newList;		//the new TokenList to be returned
	newList.isGood=false;		//assumes TokenList is not Valid

	newList.tokens = scanner(userInput);//all tokens scanned into input

	//If the user sets the ShowTokens variable to be one, the program continuously displays the tokens
	if(allValues[0]=="1"){
		cout<<"Here is a list of tokens: "<<endl;
		for(int i=0; i<newList.tokens.size(); i++)
			cout<<newList.tokens[i]<<endl;
	}

	//If the user inputs a blank line, simply returns
	if(newList.tokens.size()==0)
		return newList;//this is how we deal with lists with no entry

	//If the line isn't valid, exits from the parser
	if(!isValid(newList.tokens))
		return newList;//this is how we deal with invalid entries

	string temp="";	//used for special

	//Looking for special tokens
	for(int i = 0; i<newList.tokens.size(); i++){
		if(newList.tokens[i].substr(0,1)=="$"||newList.tokens[i].substr(1,1)=="$"){
			string whichIndex = newList.tokens[i].substr(1);
			if(whichIndex[0]=='$')
				whichIndex = whichIndex.substr(1);
			if (whichIndex[whichIndex.size()-1]=='"')
				whichIndex = whichIndex.substr(0, whichIndex.size()-1);
			if(!isFound(whichIndex)){
				cerr<<"Error: no such variable.\n";
				return newList;
				}
			else{	//if the index is found
				newList.tokens[i]=indexSearch(whichIndex);
			}//end else
		}//end if
	}//end for

	//This is where we implement the Set Function
	if(newList.tokens[0]=="set"){
		bool entered = false;		//used to determine whether to set a new variable

		for(int i = 0; i < newList.tokens[1].size(); i++){
			if(!isalpha(newList.tokens[1][i])&&!isdigit(newList.tokens[1][i])){
				cerr<<"Sorry! Invalid variable name. Please just use letters and numbers!"<<endl;
				return newList;
				}
		}

		if(allVariables.size()!=0){	//scans to find if variable has already been declared
			for(int i=0; i<allVariables.size(); i++)
				if(allVariables[i]==newList.tokens[1]){
					allValues[i]=newList.tokens[2];
					entered = true;	//if so, enters new value of the variable
					}
		}

		if(!entered){			//if variable doesn't already exist, we make a new one
			allVariables.push_back(newList.tokens[1]);
			allValues.push_back(newList.tokens[2]);
		}
	}

	newList.isGood=true;	//the list is now good if it has passed all tests
	return newList;		//returns the list
}

//Executes a preexisting function. Bool isBack is 1 if it's a background task
void execute(vector<string> command, bool isBack){
	pid_t pid;//child's id
	int status;//exit status (success or failure)
	string fullCommand="";//used for inputting into list of background jobs

	if(command[0]=="tovar")	//if tovar command, removes the second element which is the new output file
		command.erase(command.begin()+1);

	char* comArray[command.size()];	//we need to convert vector into char* for the exec command

	for(int i=1; i<command.size(); i=i+1){	//converting vector into char* and compiling the command name if it's a background command
		char *newstr =  new char[command[i].size()+1];
		strcpy (newstr, command[i].c_str());
		comArray[i-1]=newstr;
		fullCommand+=newstr;
		fullCommand+=" ";
		}

	comArray[command.size()-1]=NULL;	//char* arrays need to end in a NULL

	if((pid = fork())<0){	//If the intial fork fails, we exit with a fail
		cerr<<"Error: forking fail"<<endl;
		exit(1);
	}
	else if(pid==0){	//Executing the file immediately
		if (execvp(comArray[0], comArray) < 0){
			cerr<<"Error: exec failed."<<endl;
			exit(1);
		}
	}
	else{
		if(isBack){	//Background tasks don't call wait; also pushes it into background task array
			backJobs.push_back(fullCommand);
		}
		else{
			while(wait(&status)!=pid)	//If not a background task, processes immediately
				;
		}
	}
}

int main()
{
	string greeting = "nsh > ";	//default Greeting
	string inputLine;		//reads user input
	string temp="";
	string command;			//used to save first token of user input
	tokenList currList;		//the current token list
	allVariables.push_back("ShowTokens");	//ShowTokens variable displays all tokens if equal to 1
	allValues.push_back("0");

	while(1){

		cout<<greeting;
		if(getline(cin, inputLine)){//start checker for ./nsh < input.txt
			currList = parser(inputLine);	//parses the input line
	
			if(currList.tokens.size()==0)	//if empty
				continue;

			if(!currList.isGood)		//if input is invalid
				continue;

			command = currList.tokens[0];	//stores first token

			if(command=="done")		//exits program
				return 0;
			else if(command=="prompt")	//changes prompts
				greeting = currList.tokens[1];
			else if(command=="do")		//executes program
				execute(currList.tokens, 0);
			else if(command=="back")	//executes program in background
				execute(currList.tokens, 1);
			else if(command=="tovar"){	//executes program and reroutes output temporarily to user selected file
				char* newOutput = new char[currList.tokens[1].size()+1];
				strcpy(newOutput, currList.tokens[1].c_str());
				freopen(newOutput, "w", stdout);
				execute(currList.tokens, 0);
				fclose(stdout);
				freopen("/dev/tty", "a", stdout);
				}
			else if(command=="dir"){	//changes the working directory
				char* tempDir = new char[currList.tokens[1].size()+1];
				strcpy(tempDir, currList.tokens[1].c_str());
				if(chdir(tempDir)!=0)
					cerr<<"Invalid new directory. Pls try again.\n";
				}
			else if(command=="procs"){	//prints list of background processes
				if(backJobs.size()==0)
					cout<<"No background jobs running!\n";
				else{
					cout<<"Background jobs:\n";
					for(int i=0; i<backJobs.size(); i++)
						cout<<backJobs[i]<<endl;
				}
			}
		}//end big checker

		else{
			return 0;
		}
	}

	return 0;

}
