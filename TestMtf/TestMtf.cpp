// TestMtf.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>

int main()
{
	int cpux[4] = {0};
	__cpuidex(cpux, 0x8888, 0);
	system("pause");
    return 0;
}

