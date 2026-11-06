#pragma once

#include <vector>
#include <string>

using namespace std;

std::vector<std::string> split(const  std::string& s, const std::string& delim);
void throwException(string msg);
bool CheckMakeDir(const char *dirName);
vector<string> ReadFromFile(string fileName);
void GetFileModeAccess(string fileName, int &f_Mode, int &f_Access, int &m_Prot, int &m_Flag);
long GetFileSize(const string &filename);
int current(char *buf, int max, const char *format);
int current();
int today();