/*!
\file c_guid.h
\author	 jiangyong,13212314895@126.com
*/
#ifndef C_GUID_H
#define C_GUID_H

#include <time.h>
#include "c_netmac.h"
#include "c_md5.h"
#ifdef _WIN32
#include <process.h>
#endif
namespace ec
{
    struct t_guid
    {
        unsigned int   v1;
        unsigned short v2;
        unsigned short v3;
        unsigned char  v4[8];
    };

    class cGuid
    {
    protected:
#ifdef _WIN32
        struct t_guidinfo
        {
            FILETIME	    ts;     // ʱ��,8�ֽ�
            unsigned int    pid;    // ����pid
            unsigned int    seqno;  // �����ڵ������к�
            unsigned char   mac[8]; // ����mac��ַ
        } _uinfo;
#else
        struct t_guidinfo
        {
            timespec        ts;     // ʱ��,16�ֽ�
            unsigned int    pid;    // ����pid
            unsigned int    seqno;   // �����ڵ������к�
            unsigned char   mac[8]; // ����mac��ַ
        } _uinfo;
#endif
    public:
        cGuid() {
            memset(&_uinfo, 0, sizeof(_uinfo));
            if (!ec::getnetmac(_uinfo.mac, 1))
            {
                size_t i;
                for (i = 0; i < sizeof(_uinfo.mac); i++)
                    _uinfo.mac[i] = (unsigned char)(0xC1 + i);
            }
            _uinfo.pid = getpid();
            _uinfo.seqno = 1;
        }
        void uuid(t_guid *pguid)
        {
#ifdef _WIN32			
            GetSystemTimeAsFileTime(&_uinfo.ts);
#else
            clock_gettime(CLOCK_REALTIME, &_uinfo.ts);
#endif
            _uinfo.seqno++;
            ec::encode_md5(&_uinfo, sizeof(_uinfo), (unsigned char*)pguid);
        }

        void uuid2(unsigned char guid[16],unsigned int seqno)
        {
#ifdef _WIN32			
            GetSystemTimeAsFileTime(&_uinfo.ts);
#else
            clock_gettime(CLOCK_REALTIME, &_uinfo.ts);
#endif
            _uinfo.seqno = seqno;
            ec::encode_md5(&_uinfo, sizeof(_uinfo), guid);
        }
    };
}

#endif

