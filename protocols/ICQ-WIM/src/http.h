
class CIcqProto;

enum IcqConnection
{
	CONN_NONE = -1, CONN_MAIN = 0, CONN_FETCH = 1, CONN_RAPI = 2, CONN_LAST = 3
};

struct AsyncHttpRequest : public MTHttpRequest<CIcqProto>
{
	IcqConnection m_conn;
	MCONTACT hContact;
	char m_reqId[50];

	AsyncHttpRequest(IcqConnection, int type, const char *szUrl, MTHttpRequestHandler pFunc = nullptr);

	void ReplaceJsonParam(const char *paramName, const char *newValue);
};

/////////////////////////////////////////////////////////////////////////////////////////

class JsonReply
{
	JSONNode *m_root = nullptr;
	int m_errorCode = 0, m_detailCode = 0;
	JSONNode* m_data = nullptr;
	CMStringA m_requestId;

public:
	JsonReply(NETLIBHTTPREQUEST*);
	~JsonReply();

	__forceinline const CMStringA& requestId() const { return m_requestId; }
	__forceinline JSONNode& data() const { return *m_data; }
	__forceinline int error() const { return m_errorCode; }
	__forceinline int detail() const { return m_detailCode; }
};

class FileReply
{
	JSONNode *m_root = nullptr;
	int m_errorCode = 0;
	JSONNode* m_data = nullptr;

public:
	FileReply(NETLIBHTTPREQUEST*);
	~FileReply();

	__forceinline JSONNode& data() const { return *m_data; }
	__forceinline int error() const { return m_errorCode; }
};

class RobustReply
{
	JSONNode *m_root = nullptr;
	int m_errorCode = 0;
	JSONNode* m_results = nullptr;

public:
	RobustReply(NETLIBHTTPREQUEST*);
	~RobustReply();

	__forceinline JSONNode& results() const { return *m_results; }
	__forceinline int error() const { return m_errorCode; }
};
