/*!
\file c_wss.h
\brief websocket protocol on tls 1.2,http protocol only support get,head ; websocket protocol support Sec-WebSocket-Version:13
\date 2017.8.25

\author	 kipway@outlook.com
*/
#pragma once
#include "c_tls12.h"
#include "c_websocket.h"
namespace ec
{
    /*!
    \brief Https�����߳�
    */
    class cHttpsWorkThread : public cTlsSrvThread
    {
    public:
        cHttpsWorkThread(cTlsSession_srvMap* psss,cHttpClientMap* pclis, cHttpCfg*  pcfg, cLog*	plog) : 
            cTlsSrvThread(psss),
            _filetmp(32768), _answer(32768)
        {
            _pclis = pclis;
            _pcfg = pcfg;
            _plog = plog;
        };
        virtual ~cHttpsWorkThread() {
        };

    protected:
        cHttpCfg*		_pcfg;		//!<����
        cLog*			_plog;		//!<��־

        cHttpClientMap*	_pclis;		//!<���ӿͻ�MAP
        cHttpPacket		_httppkg;	//!<���Ľ���
        tArray<char>	_filetmp;	//!<�ļ���ʱ��
        tArray<char>	_answer;	//!<Ӧ��       
    protected:
        /*!
        \brief ����websocket���յ�������
        \return ����true��ʾ�ɹ���false��ʾʧ�ܣ��ײ��Ͽ��������
        \remark �����������������,������ܵ������ݣ������ҪӦ��ֱ��ʹ��SendToUcid����Ӧ��
        */
        virtual bool OnWebSocketData(unsigned int ucid, int bFinal, int wsopcode, const void* pdata, size_t size)//���������������websocket��������
        {
            //�򵥻��ԣ�ԭ��Ӧ����
            _answer.ClearData();
            MakeWsSend(pdata, size, (unsigned char)wsopcode, &_answer);
            SendAppData(ucid, _answer.GetBuf(), _answer.GetSize(), true);
            if (_pcfg->_blogdetail && _plog)
                _plog->AddLog("MSG:ws read:ucid=%d,Final=%d,opcode=%d,size=%d ", ucid, bFinal, wsopcode, size);
            return true;
        }
    private:
        /*!
        \brief websocket��������
        */
        bool DoUpgradeWebSocket(int ucid, const char *skey)
        {
            char sProtocol[128] = { 0 }, sVersion[128] = { 0 }, tmp[256] = { 0 };
            _httppkg.GetHeadFiled("Sec-WebSocket-Protocol", sProtocol, sizeof(sProtocol));
            _httppkg.GetHeadFiled("Sec-WebSocket-Version", sVersion, sizeof(sVersion));

            if (atoi(sVersion) < 13) //�汾��֧��С��13
            {
                if (_pcfg->_blogdetail && _plog)
                    _plog->AddLog("MSG:ws sVersion(%s) error :ucid=%d, ", sVersion, ucid);
                DoBadRequest(ucid);
                return _httppkg.HasKeepAlive();
            }
            _answer.ClearData();
            strcpy(tmp, "http/1.1 101 Switching Protocols\r\n");
            _answer.Add(tmp, strlen(tmp));

            strcpy(tmp, "Upgrade:websocket\r\nConnection:Upgrade\r\n");
            _answer.Add(tmp, strlen(tmp));

            if (sProtocol[0])
            {
                strcpy(tmp, "Sec-WebSocket-Protocol:");
                strcat(tmp, sProtocol);
                strcat(tmp, "\r\n");
                _answer.Add(tmp, strlen(tmp));
            }

            char ss[256];
            strcpy(ss, skey);
            strcat(ss, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

            char sha1out[20] = { 0 }, base64out[32] = { 0 };
            encode_sha1(ss, (unsigned int)strlen(ss), sha1out); //SHA1
            encode_base64(base64out, sha1out, 20);    //BASE64

            strcpy(tmp, "Sec-WebSocket-Accept:");
            strcat(tmp, base64out);
            strcat(tmp, "\r\n\r\n");
            _answer.Add(tmp, strlen(tmp));

            _pclis->UpgradeWebSocket(ucid);//����Э��Ϊwebsocket

            SendAppData(ucid, _answer.GetBuf(), _answer.GetSize(), true);//����

            if (_pcfg->_blogdetail && _plog) {
                _answer.Add((char)0);
                _plog->AddLog("MSG:Write ucid %d\r\n%s", ucid, _answer.GetBuf());
            }
            return true;
        }

    protected:

        /*!
        \brief WS�鷢��֡ size < 65536
        */
        bool MakeWsSend(const void* pdata, size_t size, unsigned char wsopt, tArray< char>* pout)
        {
            unsigned char uc = 0x80 | (0x0F & wsopt);
            pout->ClearData();
            pout->Add((char)uc);
            if (size < 126)
            {
                uc = (unsigned char)size;
                pout->Add((char)uc);
            }
            else if (uc < 65536)
            {
                uc = 126;
                pout->Add((char)uc);
                pout->Add((char)((size & 0xFF00) >> 8)); //���ֽ�
                pout->Add((char)(size & 0xFF)); //���ֽ�
            }
            else // < 4G
            {
                uc = 127;
                pout->Add((char)uc);
                pout->Add((char)0); pout->Add((char)0); pout->Add((char)0); pout->Add((char)0);//high 4 bytes 0
                pout->Add((char)((size & 0xFF000000) >> 24));
                pout->Add((char)((size & 0x00FF0000) >> 16));
                pout->Add((char)((size & 0x0000FF00) >> 8));
                pout->Add((char)(size & 0xFF));
            }
            pout->Add((const char*)pdata, size);
            return true;
        }

        /*!
        \brief ��Ӧping,ʹ��PONG�ش�
        */
        void OnWsPing(unsigned int ucid, const void* pdata, size_t size)
        {
            _answer.ClearData();
            MakeWsSend(pdata, size, WS_OP_PONG, &_answer);
            SendAppData(ucid, _answer.GetBuf(), _answer.GetSize(), true);
        }

        /*!
        \brief ����һ��http������,��ڲ�����_httppkg��
        \return ����true��ʾ�ɹ�������false�ᵼ�µײ�Ͽ��������
        */
        bool DoHttpRequest(unsigned int ucid)
        {
            if (_pcfg->_blogdetail && _plog)
            {
                _plog->AddLog("MSG:read from ucid %u:", ucid);
                _plog->AddLog2("   %s %s %s\r\n", _httppkg._method, _httppkg._request, _httppkg._version);
                int i, n = _httppkg._headers.GetNum();
                t_httpfileds* pa = _httppkg._headers.GetBuf();
                for (i = 0; i < n; i++)
                    _plog->AddLog2("    %s:%s\r\n", pa[i].name, pa[i].args);
                _plog->AddLog2("\r\n");
            }
            else
            {
                _plog->AddLog("MSG:ucid %u:%s %s %s", ucid, _httppkg._method, _httppkg._request, _httppkg._version);
            }
            if (!stricmp("GET", _httppkg._method)) //GET
            {
                char skey[128];
                if (_httppkg.GetWebSocketKey(skey, sizeof(skey))) //web_socket����
                {
                    if (_plog)
                        _plog->AddLog("MSG:ucid %u Upgrade websocket", ucid);
                    return DoUpgradeWebSocket(ucid, skey); //����Upgrade�е�Get
                }
                else
                    return DoGetAndHead(ucid);
            }
            else if (!stricmp("HEAD", _httppkg._method)) //HEAD
                return DoGetAndHead(ucid, false);

            DoBadRequest(ucid);//��֧����������
            return _httppkg.HasKeepAlive();
        }

        /*!
        \brief �ж��Ƿ���Ŀ¼
        */
        bool IsDir(const char* s)
        {
#ifdef _WIN32
            struct _stat st;
            if (_stat(s, &st))
                return false;
            if (st.st_mode & S_IFDIR)
                return true;
            return false;
#else
            struct stat st;
            if (stat(s, &st))
                return false;
            if (st.st_mode & S_IFDIR)
                return true;
            return false;
#endif
        }

        /*!
        \brief ȡ�ļ���չ��
        */
        const char *GetFileExtName(const char*s)
        {
            const char *pr = NULL;
            while (*s)
            {
                if (*s == '.')
                    pr = s;
                s++;
            }
            return pr;
        }

        /*!
        \brief ����GET��HEAD����
        */
        bool DoGetAndHead(unsigned int ucid, bool bGet = true)
        {
            char sfile[1024], tmp[4096];
            sfile[0] = '\0';
            tmp[0] = '\0';

            strcpy(sfile, _pcfg->_sroot);

            url2utf8(_httppkg._request, tmp, (int)sizeof(tmp));

            strcat(sfile, tmp);

            size_t n = strlen(sfile);
            if (n && (sfile[n - 1] == '/' || sfile[n - 1] == '\\')) //�����Ŀ¼��ʹ��Ĭ�ϵ�index.html��Ϊ�ļ���
                strcat(sfile, "index.html");

            else if (IsDir(sfile))
            {
                DoNotFount(ucid);
                return _httppkg.HasKeepAlive();
            }
            if (!IO::LckRead(sfile, &_filetmp))
            {
                DoNotFount(ucid);
                return _httppkg.HasKeepAlive();
            }

            _answer.ClearData();
            strcpy(tmp, "http/1.1 200 ok\r\n");
            _answer.Add(tmp, strlen(tmp));

            strcpy(tmp, "Server: rdb5 websocket server\r\n");
            _answer.Add(tmp, strlen(tmp));

            if (_httppkg.HasKeepAlive())
            {
                strcpy(tmp, "Connection: keep-alive\r\n");
                _answer.Add(tmp, strlen(tmp));
            }
            const char* sext = GetFileExtName(sfile);
            if (sext && *sext && _pcfg->GetMime(sext, tmp, sizeof(tmp)))
            {
                _answer.Add("Content-type: ", 13);
                _answer.Add(tmp, strlen(tmp));
                _answer.Add("\r\n", 2);
            }
            else
            {
                strcpy(tmp, "Content-type: application/octet-stream\r\n");
                _answer.Add(tmp, strlen(tmp));
            }

            sprintf(tmp, "Content-Length: %d\r\n\r\n", _filetmp.GetNum());
            _answer.Add(tmp, strlen(tmp));

            if (_pcfg->_blogdetail && _plog)
            {
                tArray<char> atmp(4096);
                atmp.Add(_answer.GetBuf(), _answer.GetSize());
                atmp.Add((char)0);
                _plog->AddLog("MSG:write ucid %u:", ucid);
                _plog->AddLog2("%s", atmp.GetBuf());
            }

            if (bGet) //get
                _answer.Add(_filetmp.GetBuf(), _filetmp.GetSize());

            SendAppData(ucid, _answer.GetBuf(), _answer.GetSize(), true);
            _filetmp.ClearAndFree(0xFFFFF);
            _answer.ClearAndFree(0xFFFFF);
            return true;
        }

        /*!
        \brief Ӧ��404����,��Դδ�ҵ�
        */
        void DoNotFount(unsigned int ucid)
        {
            const char* sret = "http/1.1 404  not found!\r\nServer:rdb5 websocket server\r\nConnection: keep-alive\r\nContent-type:text/plain\r\nContent-Length:9\r\n\r\nnot found";
            SendAppData(ucid, (void*)sret, (unsigned int)strlen(sret), true);
            if (_pcfg->_blogdetail && _plog)
                _plog->AddLog("MSG:write ucid %u:\r\n%s", ucid, sret);
        }

        /*!
        \brief Ӧ��400����,����ķ���
        */
        void DoBadRequest(unsigned int ucid)
        {
            const char* sret = "http/1.1 400  Bad Request!\r\nServer:rdb5 websocket server\r\nConnection: keep-alive\r\nContent-type:text/plain\r\nContent-Length:11\r\n\r\nBad Request";
            SendAppData(ucid, (void*)sret, (unsigned int)strlen(sret), true);
            if (_pcfg->_blogdetail && _plog)
                _plog->AddLog("MSG:write ucid %u:\r\n%s", ucid, sret);
        }

    protected:
        /*!
        \brief ���ؿͻ������ӶϿ���ɾ��ucid��Ӧ��Ӧ�ò�ͻ��˶���
        */        
        virtual void	OnDisconnect(unsigned int  ucid, unsigned int uopt, int nerrorcode) //uopt = TCPIO_OPT_XXXX
        {
            if (_pclis->Del(ucid) && _plog)
                _plog->AddLog("MSG:ucid %u disconnected!", ucid);
        };

        /*!
        \brief �����������
        */       
        virtual bool    OnAppData(unsigned int ucid, const void* pdata, unsigned int usize)//����false��ʾҪ�����Ҫ�Ͽ�����
        {
            bool bret = true;
            if (_pcfg->_blogdetail && _plog)
                _plog->AddLog("MSG:ucid %d read %d bytes!", ucid, usize);
            int nr = _pclis->OnReadData(ucid, (const char*)pdata, usize, &_httppkg);//�������ݣ��ṹ�����_httppkg��
            while (nr == he_ok)
            {
                if (_httppkg._nprotocol == PROTOCOL_HTTP)
                {
                    bret = DoHttpRequest(ucid);
                }
                else if (_httppkg._nprotocol == PROTOCOL_WS)
                {
                    if (_httppkg._opcode <= WS_OP_BIN)
                        bret = OnWebSocketData(ucid, _httppkg._fin, _httppkg._opcode, _httppkg._body.GetBuf(), _httppkg._body.GetSize());
                    else if (_httppkg._opcode == WS_OP_CLOSE)
                    {
                        _plog->AddLog("MSG:ucid %d WS_OP_CLOSE!", ucid);
                        return false; //����false��ײ��Ͽ�����
                    }

                    else if (_httppkg._opcode == WS_OP_PING)
                    {
                        OnWsPing(ucid, _httppkg._body.GetBuf(), _httppkg._body.GetSize());
                        if (_pcfg->_blogdetail && _plog)
                            _plog->AddLog("MSG:ucid %d WS_OP_PING!", ucid);
                        bret = true;
                    }
                }
                nr = _pclis->DoNextData(ucid, &_httppkg);
            }
            if (nr == he_failed)
            {
                DoBadRequest(ucid);
                return false;
            }
            return bret;
        };

        virtual	void	DoSelfMsg(unsigned int dwMsg) {};	// dwMsg = TCPIO_MSG_XXXX
        virtual	void	OnOptComplete(unsigned int ucid, unsigned int uopt) {};//uopt = TCPIO_OPT_XXXX
        virtual	void	OnOptError(unsigned int ucid, unsigned int uopt) {};	//uopt = TCPIO_OPT_XXXX        
    };

    /*!
    \brief httpsserver
    */
    class cHttpsServer : public cTlsServer
    {
    public:
        cHttpsServer() {};
        virtual ~cHttpsServer() {};
    public:
        cHttpCfg        _cfg;    //!<����
        cHttpClientMap	_clients;//!<���ӿͻ���
        cLog		    _log;	 //!<��־
    protected:

        virtual void    OnConnected(unsigned int  ucid, const char* sip)
        {
            cTlsServer::OnConnected(ucid,sip);
            _log.AddLog("MSG:ucid %u TCP connected from IP:%s!", ucid, sip);
            _clients.Add(ucid, sip);
        };
        virtual void	OnRemovedUCID(unsigned int ucid)
        {            
            if (_clients.Del(ucid))
                _log.AddLog("MSG:ucid %u disconnected!", ucid);
            cTlsServer::OnRemovedUCID(ucid);
        };
        virtual void    CheckNotLogin() {};
    public:
        virtual ec::cTcpSvrWorkThread* CreateWorkThread()
        {
            cHttpsWorkThread* pthread = new cHttpsWorkThread(&_sss, &_clients, &_cfg, &_log);
            return pthread;           
        }
    public:
        bool Init(const char* scfgfile, const char* filecert, const char* filerootcert, const char* fileprivatekey)
        {
            if (!_cfg.ReadIniFile(scfgfile))
                return false;
            return InitCert(filecert, filerootcert, fileprivatekey);
        }

        bool StartServer(unsigned int uThreads, unsigned int  uMaxConnect)
        {
            if (!_log.Start(_cfg._slogpath))
                return false;
            return Start(_cfg._wport, uThreads, uMaxConnect);
        }
        void StopServer()
        {
            Stop();
            _log.AddLog("MSG:httpsrv stop success!");
            _log.Stop();
        }
    };
}//ec
