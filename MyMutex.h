#pragma once
#include<Windows.h>

class MyMutex
{
public:
	MyMutex();
	~MyMutex();
	void Acquire();
	void Release();
	void SleepConditionVariableCS(DWORD dwMilliseconds);
	void WakeAllConditionVariable();
private:
	CRITICAL_SECTION cs;
	CONDITION_VARIABLE cv;
	
};

