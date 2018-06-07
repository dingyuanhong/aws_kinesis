
#include "log.h"

const char * getLevelString(int level)
{
	switch(level)
	{
		case LOG_VERBOSE:// 0x1
			return "VERBOSE";
		case LOG_DEBUG:// 0x2	//调试
			return "DEBUG";
		case LOG_INFO:// 0x4	//信息
			return "INFO";
		case LOG_NOTICE:// 0x8 //通知
			return "NOTICE";
		case LOG_WARN:// 0x10	//警告
			return "WARN";
		case LOG_ALERT:// 0x20  //警报
			return "ALERT";
		case LOG_ERROR:// 0x40  //错误
			return "ERROR";
		case LOG_ASSERT:// 0x80 //断言
			return "ASSERT";
		case LOG_ABORT:// 0x400	//终止
			return "ABORT";
	}

	return "";
}

void log_default_printf(const char * file,int line,const char * func,int level,const char *format,...)
{
	if(func != NULL && line != -1)
	{
		printf("%s(%s %d):",getLevelString(level),func,line);
	}
	va_list list;
   	va_start(list,format);
	vprintf(format,list);
	va_end(list);
}

LOG_PRINTF G_LOG_PRINTF = log_default_printf;

LOG_PRINTF set_log_printf(LOG_PRINTF log)
{
	LOG_PRINTF old = G_LOG_PRINTF;
	G_LOG_PRINTF = log;
	return old;
}
