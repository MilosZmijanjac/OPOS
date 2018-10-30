#include "MyMutex.h"



MyMutex::MyMutex()
{
	::InitializeCriticalSection(&cs);
	::InitializeConditionVariable(&cv);
}


MyMutex::~MyMutex()
{
	::DeleteCriticalSection(&cs);
}

void MyMutex::Acquire()
{
	::EnterCriticalSection(&cs);
}

void MyMutex::Release()
{
	::LeaveCriticalSection(&cs);
	
}

void MyMutex::SleepConditionVariableCS(DWORD dwMilliseconds)
{
	::SleepConditionVariableCS(&cv, &cs, dwMilliseconds);
}

void MyMutex::WakeAllConditionVariable()
{
	::WakeAllConditionVariable(&cv);
}
