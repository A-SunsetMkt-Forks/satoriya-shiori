#include	"satori.h"
#include	"../_/Utilities.h"
#include "posix_utils.h"
#include	<time.h>
#include	<tlhelp32.h>
#include	<sstream>

#include "random.h"

//////////DEBUG/////////////////////////
#include "warning.h"
#ifdef _WINDOWS
#ifdef _DEBUG
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif
////////////////////////////////////////

void	add_characters(const char* p, int& chars_spoken) {
	// さくらスクリプトとそれ以外を分割して処理を加える
	while (*p) {
		if (*p=='\\'||*p=='%') {
			++p;
			if (*p=='\\'||*p=='%')	// エスケープされた\, %
				continue;
			while (!_ismbblead(*p) && (isalpha(*p)||isdigit(*p)||*p=='!'||*p=='*'||*p=='&'||*p=='?'||*p=='_'))
				++p;
			if (*p=='[') {
				p += 1;
				while ( *p && *p!=']' ) {
					if (p[0]=='\\' && p[1]==']') {	// エスケープされた]
						++p;
					}
					else {
						p += _ismbblead(*p) ? 2 : 1;
					}
				}
			}
		}
		else {
			int len = _ismbblead(*p) ? 2 : 1;
			p += len;
			chars_spoken += len;
		}
	}
}

/*#ifndef POSIX
static	SYSTEMTIME	DwordToSystemTime(DWORD dw) {
	SYSTEMTIME	st = { 0, 0, 0, 0, 0, 0, 0, 0 };
	st.wMilliseconds=WORD(dw%1000); dw/=1000;
	st.wSecond=WORD(dw%60); dw/=60;
	st.wMinute=WORD(dw%60); dw/=60;
	st.wHour=WORD(dw);
	return	st;
}
#endif*/

//get_property関数用のハンドラと結果格納
#ifndef POSIX
std::string execute_result;
LRESULT CALLBACK GetPropertyHandler(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (message == WM_COPYDATA)
	{
		const COPYDATASTRUCT* cds = (const COPYDATASTRUCT*)lparam;
		string recv_str((const char*)cds->lpData, cds->cbData);
		
		string header = cut_token(recv_str, CRLF);
		cut_token(header, " ");
		if (header == "200 OK")
		{
			//1行文読み捨て
			cut_token(recv_str, CRLF);
			execute_result = cut_token(recv_str, CRLF);
		}
	}
	return CallWindowProc(DefWindowProc, hwnd, message, wparam, lparam);
}
#endif

string	Satori::inc_call(
	const string& iCallName, 
	const strvec& iArgv, 
	strvec& oResults, 
	bool iIsSecure) 
{

	if ( iCallName == "バイト値" ) {
		if ( iArgv.size() ) {
			char bytes[2] = {0,0};
			bytes[0] = zen2int(iArgv[0]);
			return bytes;
		}
		else {
			GetSender().sender() << "error: 'バイト値' : 引数が不正です。" << std::endl;
			return "";
		}
	}

	if ( iCallName=="nop" ) {
		return "";
	}

	if ( iCallName == "合成単語群" ) {
		if ( iArgv.size() ) {
			std::vector<const Word*> vt;
			for ( strvec::const_iterator it = iArgv.begin() ; it != iArgv.end() ; ++it ) {
				words.select_all(*it,*this,vt);
			}
			if ( vt.size() ) {
				return *(vt[random(vt.size())]);
			}
			else {
				return "";
			}
		}
		else {
			GetSender().sender() << "error: '合成単語群' : 引数が不正です。" << std::endl;
			return "";
		}
	}

	if ( !iIsSecure ) {
		GetSender().sender() << "local/Localでないので蹴りました: " << iCallName << std::endl;
		return	"";
	}

	if ( iCallName=="set" ) {
		if ( iArgv.size()==2 ) {
			string	result, key=iArgv[0], value=iArgv[1];

			SubstVariable(key,value,result,false);

			return	result;
		}
		return	"";
	}
	
	if ( iCallName=="loop" ) {
		int	init=1, max=0, step=1, arg_size=iArgv.size();
		if ( arg_size==2 ) {
			max=zen2int(iArgv[1]);
		}
		else if ( arg_size==3 ) {
			init=zen2int(iArgv[1]);
			max=zen2int(iArgv[2]);
		}
		else if ( arg_size==4 ) {
			init=zen2int(iArgv[1]);
			max=zen2int(iArgv[2]);
			step=zen2int(iArgv[3]);
		}
		else
			return	"";
		string	name=iArgv[0];
		string	ret,temp;

		if ( step==0 )
			return	"";
		else if ( step>0 ) {
			if ( init>max )
				return	"";
			for (int i=init ; i<=max ; i+=step ) {
				variables[name+"カウンタ"] = itos(i);
				if ( !Call(name, temp) )
					return	"";
				ret += temp;
			}
		}
		else {
			if ( init<max )
				return	"";
			for (int i=init ; i>=max ; i+=step ) {
				variables[name+"カウンタ"] = itos(i);
				if ( !Call(name, temp) )
					return	"";
				ret += temp;
			}
		}
		variables.erase(name+"カウンタ");
		return	ret;
	}
	
	if ( iCallName=="sync" ) {
		string	str = "\\![raise,OnDirectSaoriCall";
		if ( !iArgv.empty() ) {
			string	arg;
			combine(arg, iArgv, ",");
			str += ",";
			str += arg;
		}
		str += "]";
		return	str;
	}
	
	if ( iCallName=="remember" ) {
		if ( iArgv.size() == 1 ) {
			int	n = zen2int(iArgv[0]);
			if ( mResponseHistory.size() > n ) {
				return	mResponseHistory[n];
			}
		}
		return	"";
	}
	
	if ( iCallName=="call" ) {
		if ( iArgv.size() >= 1 ) {
			mCallStack.push( strvec() );
			strvec&	v = mCallStack.top();
			for ( int i=1 ; i<iArgv.size() ; ++i )
				v.push_back( iArgv[i] );
			string	r;
			Call(iArgv[0],r,false,false,true);
			mCallStack.pop();
			return	r;
		}
		return	"";
	}

	if (iCallName == "vncall")
	{
		if (iArgv.size() >= 1) {
			
			strvec	v;// = mCallStack.top();
			string	r;

			for (int i = 1; i < iArgv.size(); ++i){
				Call(iArgv[i], r);
				v.push_back(r);
			}
//				v.push_back(*GetValue(iArgv[i], ex));

			mCallStack.push(v);		//ここでpushしてはだめ、呼び出し先でA0取るとすると、pushのタイミングはcallの直前。
			Call(iArgv[0], r, false, false, true);
			mCallStack.pop();
			return	r;
		}
		return	"";
	}

	if (iCallName == "get_property")
	{
		if (iArgv.size() >= 1)
		{
			//里々からベースウェアにDirectSSTPを飛ばしてプロパティシステムにアクセスする。元のSHIORI呼出を返さずにプロパティを取れる。
#ifndef POSIX
			//リザルトのリセット
			execute_result = "";

			//結果受信用ウインドウ作成: リソースの仕様を局所化してみたけどオーバーヘッドがでかい場合はSHIORIの初期化周辺に絡めるといいのかも
			const char* windowname = "satori_get_property";
			
			WNDCLASSEX windowClass;
			ZeroMemory(&windowClass,sizeof(windowClass));

			windowClass.cbSize = sizeof(windowClass);
			windowClass.hInstance = GetModuleHandle(NULL);
			windowClass.lpszClassName = windowname;
			windowClass.lpfnWndProc = ::GetPropertyHandler;
			
			::RegisterClassEx(&windowClass);

			HWND propertyWindow = ::CreateWindow(windowname, windowname, 0, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

			//リクエスト作成
			const HWND targetHWnd = characters_hwnd[0];
			std::ostringstream ost;
			ost << "EXECUTE SSTP/1.1\r\nCommand: GetProperty[" << iArgv[0] << "]\r\nSender: Satori\r\nCharset: Shift_JIS\r\n\r\n";
			std::string sendData = ost.str();

			//メッセージ転送
			COPYDATASTRUCT cds;
			cds.dwData = 9801;
			cds.cbData = sendData.size();
			cds.lpData = malloc(cds.cbData);
			memcpy(cds.lpData, sendData.c_str(), cds.cbData);

			/*LRESULT res =*/ ::SendMessage(targetHWnd, WM_COPYDATA, (WPARAM)propertyWindow, (LPARAM)&cds);
			
			//リソースの開放
			free(cds.lpData);
			
			::DestroyWindow(propertyWindow);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);

			return execute_result;
#else
			return "";
#endif
		}
	}

	if (iCallName == "load_saori")
	{
		if (iArgv.size() >= 2)
		{
			string load_line = iArgv[0];
			for (int i = 1; i < iArgv.size(); ++i)
				load_line += "," + iArgv[i];

			mShioriPlugins->load_a_plugin(load_line);
		}
	}

	if (iCallName == "equal") {
		if (iArgv.size() == 2) {
			const string &lhs = iArgv[0], &rhs = iArgv[1];
			return itos(lhs == rhs);
		}
		return	"";
	}
	
	if ( iCallName == "単語の追加" ) {

		if ( iArgv.size() == 2 )
		{
			Family<Word>* f = words.get_family(iArgv[0]);
			if ( f == NULL || false == f->is_exist_element(iArgv[1]) )
			{
				mAppendedWords[ iArgv[0] ].push_back( words.add_element(iArgv[0],iArgv[1],Condition()) );
				GetSender().sender() << "単語群「" << iArgv[0] << "」に単語「" << iArgv[1] << "」が追加されました。" << std::endl;
			}
			else
			{
				GetSender().sender() << "単語群「" << iArgv[0] << "」に単語「" << iArgv[1] << "」は既に存在します。" << std::endl;
			}
		}
		else {
			GetSender().sender() << "error: '単語の追加' : 引数が不正です。" << std::endl;
		}
		return	"";
	}

	if ( iCallName == "追加単語の削除" ) {
		if ( iArgv.size() == 2 )
		{
			Family<Word>* f = words.get_family(iArgv[0]);
			if ( f && f->is_exist_element(iArgv[1]) ) { //すでに存在し…
				std::map<string, std::vector<Word> >::iterator it = mAppendedWords.find(iArgv[0]);
				if ( it != mAppendedWords.end() ) { //しかも「単語の追加」で追加したもので…
					std::vector<Word> &setword = it->second;
					
					std::vector<Word>::iterator itrm = std::remove(setword.begin(),setword.end(),iArgv[1]);
					if ( itrm != setword.end() ) {
						setword.erase(itrm,setword.end());
						f->delete_element(iArgv[1]);

						if ( setword.empty() ) {
							mAppendedWords.erase(it);
						}
						GetSender().sender() << "単語群「" << iArgv[0] << "」の単語「" << iArgv[1] << "」が削除されました。" << std::endl;
						if ( f->empty() ) {
							words.erase(iArgv[0]);
						}
					}
				}
			}
		}
		else {
			GetSender().sender() << "error: '追加単語の削除' : 引数が不正です。" << std::endl;
		}
		return	"";
	}
	
	if ( iCallName == "追加単語の全削除" ) {
		if ( iArgv.size() == 1 )
		{
			Family<Word>* f = words.get_family(iArgv[0]);
			if ( f ) { //すでに存在し…
				std::map<string, std::vector<Word> >::iterator it = mAppendedWords.find(iArgv[0]);
				if ( it != mAppendedWords.end() ) { //しかも「単語の追加」で追加したもので…
					std::vector<Word> &setword = it->second;
					for ( std::vector<Word>::const_iterator its = setword.begin(); its != setword.end() ; ++its ) {
						f->delete_element(*its);
					}
					mAppendedWords.erase(it);

					GetSender().sender() << "単語群「" << iArgv[0] << "」に追加された単語は全て削除されました。" << std::endl;
				}
				if ( f->empty() ) {
					words.erase(iArgv[0]);
				}
			}
		}
		else {
			GetSender().sender() << "error: '単語の削除' : 引数が不正です。" << std::endl;
		}
		return	"";
	}
	return	"";
}

// R・H・A・S・Cを判定
bool Satori::IsArrayValue(const string &iName,int &ref,char &firstChar)
{
	if ( ((iName[0]=='R' || iName[0]=='H' || iName[0]=='A' || iName[0]=='S' || iName[0]=='C') && iName.size() >= 2) ||
		((iName.compare(0,2,"Ｒ") == 0 || iName.compare(0,2,"Ｈ") == 0 ||
		iName.compare(0,2,"Ａ") == 0 || iName.compare(0,2,"Ｓ") == 0 ||
		iName.compare(0,2,"Ｃ") == 0) && iName.size() >= 3) ) {

		string hankaku=zen2han(iName);

		if ( aredigits(hankaku.c_str()+1) ) {
			firstChar = hankaku[0];
			ref = stoi_internal(hankaku.c_str()+1);
			return true;
		}
	}
	return false;
}


// 変数取得
string* Satori::GetValue(const string &iName,bool &oIsSysValue,bool iIsExpand,bool *oIsExpanded,const char *pDefault)
{
	if ( oIsExpanded ) { *oIsExpanded = false; }
	oIsSysValue = false;

	int ref;
	char firstChar;

	if ( IsArrayValue(iName,ref,firstChar) ) {
		if ( firstChar=='R' ) {
			oIsSysValue = true;

			// Event通知時の引数取得
			if (ref>=0 && ref<mReferences.size()) {
				return &(mReferences[ref]);
			}
			else {
				if ( iIsExpand && ref >= 0 ) {
					mReferences.resize(ref+1);
					if ( oIsExpanded ) { *oIsExpanded = true; }
					return &(mReferences[ref]);
				}
				return NULL;
			}
		}
		else if ( firstChar=='H' ) {
			oIsSysValue = true;

			// 過去の置き換え履歴を参照
			if ( kakko_replace_history.empty() ) { return NULL; }

			strvec&	khr = kakko_replace_history.top();

			ref -= 1; //いっこまえでないと履歴にならん！

			if ( ref>=0 && ref < khr.size() ) {
				return &(khr[ref]);
			}
			else {
				return NULL;
			}
		}
		else if ( firstChar=='A' ) {
			oIsSysValue = true;

			if ( mCallStack.empty() ) { return NULL; }

			// callによる呼び出しの引数を参照S
			strvec&	v = mCallStack.top();
			if ( ref >= 0 && ref < v.size() ) {
				return &(v[ref]);
			}
			else {
				return NULL;
			}
		}
		else if ( firstChar=='S' ) {
			oIsSysValue = true;

			// SAORIなどコール時の結果処理
			if (ref>=0 && ref<mKakkoCallResults.size()) {
				return &(mKakkoCallResults[ref]);
			}
			else {
				if ( iIsExpand && ref >= 0 ) {
					mKakkoCallResults.resize(ref+1);
					if ( oIsExpanded ) { *oIsExpanded = true; }
					return &(mKakkoCallResults[ref]);
				}
				return NULL;
			}
		}
		else if ( firstChar=='C' ) {
			oIsSysValue = true;

			if ( 0 <= ref && ref < mLoopCounters.size() ) {
				return &(mLoopCounters.from_top(ref));
			}
			else {
				return NULL;
			}
		}
	}

	if ( variables.find(iName) != variables.end() ) {
		// 変数名であれば変数の内容を返す
		return &(variables[iName]);
	}

	if ( iIsExpand ) {
		variables[iName] = string(pDefault);
		if ( oIsExpanded ) { *oIsExpanded = true; }
		return &(variables[iName]);
	}
	return NULL;
}


// 引数に渡されたものを何かの名前であるとし、置き換え対象があれば置き換える。
bool	Satori::Call(const string& iName, string& oResult, bool for_calc, bool for_non_talk, bool use_arg_callstack)
{
	if ( for_calc ) {
		for_non_talk = true;
	}

	++m_nest_count;

	if ( m_nest_limit > 0 && m_nest_count > m_nest_limit ) {
		GetSender().sender() << "呼び出し回数超過：" << iName << std::endl;
		oResult = "（" + iName + "）";
		--m_nest_count;
		return false;
	}

	bool r = CallReal(iName,oResult,for_calc,for_non_talk, use_arg_callstack);
	--m_nest_count;

	if ( r && oResult.empty() ) {
		if ( for_calc ) {
			oResult = "０";
		}
	}
	return r;
}

bool	Satori::CallReal(const string& iName, string& oResult, bool for_calc, bool for_non_talk, bool use_arg_callstack)
{
	simple_stack<strvec>::size_type stack_size_before_call = kakko_replace_history.size();

	bool	_pre_called_=false;

	// SAORI対応, 内蔵関数呼び出しもここで
	{
		string	thePluginName="";
		std::set<string>::const_iterator theDelimiter = mDelimiters.end();

		const char* p = NULL;
		enum { NO_CALL, SAORI_CALL, INC_CALL, SPECIAL_CALL } state = NO_CALL;

		if ( mShioriPlugins->find(iName) ) {
			thePluginName=iName;
			state = SAORI_CALL;
		} else {

			static std::set<string> inner_commands;
			if ( inner_commands.empty() ) {
				// 本当はstd::map<name, function>だなー　むー
				inner_commands.insert("set");
				inner_commands.insert("get_property");
				inner_commands.insert("nop");
				inner_commands.insert("sync");
				inner_commands.insert("loop");
				inner_commands.insert("remember");
				inner_commands.insert("call");
				inner_commands.insert("vncall");
				inner_commands.insert("equal");
				inner_commands.insert("バイト値");
				inner_commands.insert("文の数");
				inner_commands.insert("単語の追加");
				inner_commands.insert("合成単語群");
				inner_commands.insert("追加単語の削除");
				inner_commands.insert("追加単語の全削除");
			}

			if (use_arg_callstack)
			{
				//コールスタックを引数として使う場合、区切り文字が無いので
				if (mShioriPlugins->find(iName.c_str())) {	// 存在確認
					thePluginName = iName.c_str();
					state = SAORI_CALL;
				}
				else if (special_commands.find(iName.c_str()) != special_commands.end()){
					thePluginName = iName.c_str();
					state = SPECIAL_CALL;
				}
				else if (inner_commands.find(iName.c_str()) != inner_commands.end()) {
					thePluginName = iName.c_str();
					state = INC_CALL;
				}
			}
			else
			{
				for (std::set<string>::const_iterator i = mDelimiters.begin(); i != mDelimiters.end(); ++i) {
					p = strstr_hz(iName.c_str(), i->c_str());
					if (p == NULL)
						continue;
					string	str(iName.c_str(), p - iName.c_str());
					if (mShioriPlugins->find(str)) {	// 存在確認
						thePluginName = str;
						theDelimiter = i;
						state = SAORI_CALL;
						break;
					}
					else if (special_commands.find(str) != special_commands.end()){
						thePluginName = str;
						theDelimiter = i;
						state = SPECIAL_CALL;
						break;
					}
					else if (inner_commands.find(str) != inner_commands.end()) {
						thePluginName = str;
						theDelimiter = i;
						state = INC_CALL;
						break;
					}
				}
			}
		}

		if ( state==NO_CALL ) {
			_pre_called_=false;
		}
		else
		{
			_pre_called_=true;
			strvec	theArguments;

			if (use_arg_callstack && !mCallStack.empty())
			{
				//call / vncall用の処理。
				//SPECIAL CALL の動作は保証しません
				theArguments = mCallStack.top();
			}
			else
			{
				if (p != NULL)// 引数があるなら
				{
					assert(theDelimiter != mDelimiters.end());

					if (state == SPECIAL_CALL) {
						int level = 0;
						get_a_chr(p);
						const char *p_start = p;
						while (true){
							if (*p == '\0'){
								theArguments.push_back(string(p_start, p - p_start));
								break;
							}
							string c = get_a_chr(p);
							if (c == "（") {
								level++;
							}
							if (c == "）") {
								level--;
							}
							if (level < 0) {
								theArguments.push_back(string(p_start, p - p_start - 2));
								break;
							}
							if (level == 0) {
								if (c == *theDelimiter) {
									theArguments.push_back(string(p_start, p - p_start - c.size()));
									p_start = (char *)p;
								}
							}
						}
					}
					else{
						string argstr = UnKakko(p, false, true);
						while (true)
						{
							p += theDelimiter->size();
							const char* pdlmt = strstr_hz(p, theDelimiter->c_str());
							if (pdlmt == NULL) {
								theArguments.push_back(p);
								break;
							}
							theArguments.push_back(string(p, pdlmt - p));
							p = pdlmt;
						}

						if (mSaoriArgumentCalcMode != SACM_OFF) {
							for (strvec::iterator i = theArguments.begin(); i != theArguments.end(); ++i) {
								if (i->size() == 0)
									continue;
								if (mSaoriArgumentCalcMode == SACM_AUTO) {
									int	c = zen2han(*i).at(0);
									if (c != '+' && c != '-' && !(c >= '0' && c <= '9'))
										continue;
								}

								string	exp = *i;
								if (calc(exp, true)) {
									if (state == SAORI_CALL && aredigits(zen2han(exp))) {
										*i = zen2han(exp);
									}
									else {
										*i = exp;
									}
								}
							}
						}

					}
				}
			}

			

			// 引数渡して返値を取得、と。
			if ( state==SAORI_CALL ) {
				for ( strvec::iterator i=theArguments.begin() ; i!=theArguments.end() ; ++i ) {
					m_escaper.unescape(*i);
				}
				oResult = mShioriPlugins->request(thePluginName, theArguments, mKakkoCallResults, secure_flag ? "Local" : "External" );
			}
			else if ( state==SPECIAL_CALL ) {
				oResult = special_call(thePluginName, theArguments, false, true, secure_flag);
			}
			else {
				oResult = inc_call(thePluginName, theArguments, mKakkoCallResults, secure_flag);
			}
			oResult = UnKakko(oResult.c_str());	// 返値を再度カッコ展開
		}
	}

	const Word* w;
	string hankaku=zen2han(iName);
	bool isSysValue;
	string *pstr = GetValue(iName,isSysValue);

	if ( _pre_called_ ) {
		// 前段階ですでに対応カッコ展開済み
	}
	else if ( (w = words.select(iName, *this)) != NULL )
	{
		// 単語を選択した
		GetSender().sender() << "＠" << iName << std::endl;

		if ( talks.is_exist(iName) ) {
			GetSender().sender() << "同じ名前「" << iName << "」の単語群と文があります。トラブルの元なので避けましょう。" << std::endl;
		}

		oResult = UnKakko( w->c_str() );
		//括弧展開後にチェックするようになったのでここは無効化
		//if ( ! for_non_talk ) {
		//	if ( oResult.size() ) {
		//		speaked_speaker.insert(speaker);
		//		add_characters(oResult.c_str(), chars_spoken);
		//	}
		//}
	}
	else if ( talks.is_exist(iName) ) {
		// ＊に定義があれば文を取得
		oResult = GetSentence(iName);
	}
	else if ( pstr || isSysValue ) {
		// 変数名であれば変数の内容を返す
		if ( pstr ) {
			oResult = *pstr;
		}
		else {
			oResult = "";
		}
	}
	else if ( aredigits(hankaku) || (hankaku[0]=='-' && aredigits(hankaku.c_str()+1)) ) {
		// サーフェス切り替え
		int	s = stoi_internal(hankaku);
		oResult = string("\xff\x02") + itos(s) + "\xff"; //内部特殊表現に一旦変換して、後でサーフェス加算処理をする
		/* 展開後に処理される
		if ( !is_speaked(speaker) ) {
			if ( surface_changed_before_speak.find(speaker) == surface_changed_before_speak.end() ) {
				surface_changed_before_speak.insert(std::map<int,bool>::value_type(speaker,is_speaked_anybody()) );
			}
		}*/
	}
	else if ( hankaku=="Aの数" ) {
		if ( ! mCallStack.empty() ) {
			oResult = itos(mCallStack.top().size());
		}
		else {
			oResult = "0";
		}
	}
	else if ( hankaku=="Rの数" ) {
		oResult = itos(mReferences.size());
	}
	else if ( hankaku=="Sの数" ) {
		oResult = itos(mKakkoCallResults.size());
	}
	else if ( strncmp(iName.c_str(), "乱数", 4)==0 && iName.size()>6 ) { 
		strvec	vec;
		if ( split( iName.c_str()+4, "〜", vec ) != 2 ) {
			oResult = "※　乱数の指定が変です　※";
		}
		else {
			string vec0 = zen2han(vec[0]);
			int	bottom = stoi_internal(vec0);
			int	top = zen2int(vec[1]);
			if ( bottom > top )
				Swap(&bottom, &top);

			if ( vec0 != vec[0] ) {
				if ( bottom == top )
					oResult = int2zen(top);
				else 
					oResult = int2zen( random(top-bottom+1) + bottom );
			}
			else {
				if ( bottom == top )
					oResult = itos(top);
				else 
					oResult = itos( random(top-bottom+1) + bottom );
			}
		}
	}
	else if ( iName == "里々のバージョン" ) {
		oResult = gSatoriVersion;
	}
	else if ( iName == "里々のライセンス" ) {
		oResult = gSatoriLicense;
		replace(oResult, "\n", "\\n");
	}
	else if ( iName == "現在年" ) {
#ifdef POSIX
	        time_t st = time(NULL);
	        oResult = int2zen(localtime(&st)->tm_year + 1900);
#else
		SYSTEMTIME st; ::GetLocalTime(&st); oResult=int2zen(st.wYear);
#endif
	}
	else if ( iName == "現在曜日" ) {
#ifdef POSIX
	        time_t st = time(NULL);
		struct tm* st_tm = localtime(&st);
		static const char* const ary[7]={"日","月","火","水","木","金","土"};
		oResult = (st_tm->tm_wday >= 0 && st_tm->tm_wday < 7) ? ary[st_tm->tm_wday] : "？";
#else
		SYSTEMTIME st; ::GetLocalTime(&st);
		static const char* const ary[7]={"日","月","火","水","木","金","土"};
		oResult = ( st.wDayOfWeek >= 0 && st.wDayOfWeek < 7 ) ? ary[st.wDayOfWeek] : "？";
#endif
	}
#ifdef POSIX
	else if ( iName == "現在月" ) { time_t st = time(NULL); oResult = int2zen(localtime(&st)->tm_mon + 1); }
	else if ( iName == "現在日" ) { time_t st = time(NULL); oResult = int2zen(localtime(&st)->tm_mday); }
	else if ( iName == "現在時" ) { time_t st = time(NULL); oResult = int2zen(localtime(&st)->tm_hour); }
	else if ( iName == "現在分" ) { time_t st = time(NULL); oResult = int2zen(localtime(&st)->tm_min); }
	else if ( iName == "現在秒" ) { time_t st = time(NULL); oResult = int2zen(localtime(&st)->tm_sec); }
#else
	else if ( iName == "現在月" ) { SYSTEMTIME st; ::GetLocalTime(&st); oResult=int2zen(st.wMonth); }
	else if ( iName == "現在日" ) { SYSTEMTIME st; ::GetLocalTime(&st); oResult=int2zen(st.wDay); }
	else if ( iName == "現在時" ) { SYSTEMTIME st; ::GetLocalTime(&st); oResult=int2zen(st.wHour); }
	else if ( iName == "現在分" ) { SYSTEMTIME st; ::GetLocalTime(&st); oResult=int2zen(st.wMinute); }
	else if ( iName == "現在秒" ) { SYSTEMTIME st; ::GetLocalTime(&st); oResult=int2zen(st.wSecond); }
#endif
	//起動
	else if (iName == "起動時") {
	    time_t sec = posix_get_current_sec() - sec_count_at_load;
	    time_t hour = sec / 60 / 60;
	    oResult = int2zen(hour);
	}
	else if (iName == "起動分") {
	    time_t sec = posix_get_current_sec() - sec_count_at_load;
	    time_t hour = sec / 60 / 60;
	    sec -= hour * 60 * 60;
	    time_t minute = sec / 60;
	    oResult = int2zen(minute);
	}
	else if (iName == "起動秒" ) {
	    time_t sec = posix_get_current_sec() - sec_count_at_load;
	    time_t hour = sec / 60 / 60;
	    sec -= hour * 60 * 60;
	    time_t minute = sec / 60;
	    sec -= minute * 60;
	    oResult = int2zen(sec);
	}
	else if (iName == "単純起動秒" ) {
	    time_t sec = posix_get_current_sec() - sec_count_at_load;
	    oResult = int2zen(sec);
	}
	else if (iName == "単純起動分") {
	    time_t sec = posix_get_current_sec() - sec_count_at_load;
	    oResult = int2zen(sec / 60);
	}
	else if (iName == "単純起動時") {
	    time_t sec = posix_get_current_sec() - sec_count_at_load;
	    oResult = int2zen(sec / 60 / 60);
	}
	//OS起動
	else if (iName == "OS起動時" || iName == "ＯＳ起動時") {
	    time_t sec = posix_get_current_sec();
	    time_t hour = sec / 60 / 60;
	    oResult = int2zen(hour);
	}
	else if (iName == "OS起動分" || iName == "ＯＳ起動分" ) {
	    time_t sec = posix_get_current_sec();
	    time_t hour = sec / 60 / 60;
	    sec -= hour * 60 * 60;
	    time_t minute = sec / 60;
	    oResult = int2zen(minute);
	}
	else if (iName == "OS起動秒" || iName == "ＯＳ起動秒") {
	    time_t sec = posix_get_current_sec();
	    time_t hour = sec / 60 / 60;
	    sec -= hour * 60 * 60;
	    time_t minute = sec / 60;
	    sec -= minute * 60;
	    oResult = int2zen(sec);
	}
	else if (iName == "単純OS起動秒" || iName == "単純ＯＳ起動秒") {
	    time_t sec = posix_get_current_sec();
	    oResult = int2zen(sec);
	}
	else if (iName == "単純OS起動分" || iName == "単純ＯＳ起動分") {
	    time_t sec = posix_get_current_sec();
	    oResult = int2zen(sec / 60);
	}
	else if (iName == "単純OS起動時" || iName == "単純ＯＳ起動時") {
	    time_t sec = posix_get_current_sec();
	    oResult = int2zen(sec / 60 / 60);
	}
	//累計
	else if (iName == "累計時") {
	    unsigned long sec = posix_get_current_sec() - sec_count_at_load + sec_count_total;
		unsigned long hour = sec / 60 / 60;
	    oResult = ul2zen(hour);
	}
	else if (iName == "累計分" ) {
		unsigned long  sec = posix_get_current_sec() - sec_count_at_load + sec_count_total;
		unsigned long  hour = sec / 60 / 60;
	    sec -= hour * 60 * 60;
		unsigned long  minute = sec / 60;
		oResult = ul2zen(minute);
	}
	else if (iName == "累計秒") {
		unsigned long  sec = posix_get_current_sec() - sec_count_at_load + sec_count_total;
		unsigned long  hour = sec / 60 / 60;
	    sec -= hour * 60 * 60;
		unsigned long  minute = sec / 60;
	    sec -= minute * 60;
		oResult = ul2zen(sec);
	}
	else if (iName == "単純累計秒") {
		unsigned long  sec = posix_get_current_sec() - sec_count_at_load + sec_count_total;
		oResult = ul2zen(sec);
	}
	else if (iName == "単純累計分") {
		unsigned long  sec = posix_get_current_sec() - sec_count_at_load + sec_count_total;
		oResult = ul2zen(sec / 60);
	}
	else if (iName == "単純累計時") {
		unsigned long  sec = posix_get_current_sec() - sec_count_at_load + sec_count_total;
		oResult = ul2zen(sec / 60 / 60);
	}
	else if ( hankaku == "time_t" ) { time_t tm; time(&tm); oResult=int2zen(tm); }
	else if ( iName == "最終トークからの経過秒" ) { oResult=int2zen(second_from_last_talk); }

	else if ( compare_head(iName, "サーフェス") && aredigits(iName.c_str()+10) ) {
		oResult=itos(cur_surface[ zen2int(iName.c_str()+10) ]);
	}
	else if ( compare_head(iName, "前回終了時サーフェス") && iName.length() > 20 ) {
		oResult=itos(last_talk_exiting_surface[ zen2int(iName.c_str()+20) ]);
	}

#ifndef POSIX
	else if ( compare_head(iName, "ウィンドウハンドル") && iName.length() > 18 ) {
		int character = zen2int(iName.c_str()+18);
		std::map<int,HWND>::const_iterator found = characters_hwnd.find(character);
		if ( found != characters_hwnd.end() ) {
			oResult = uitos((unsigned int)found->second);
		}
	}
#endif

	else if ( iName == "隣で起動しているゴースト" ) { 
		oResult = ( otherghostname.size()>=1 ) ? *otherghostname.begin() : ""; //自分自身はotherghostnameには含まない
	}
	else if ( iName == "起動しているゴースト数" ) { 
		oResult = int2zen(otherghostname.size()+1); //自分自身はotherghostnameには含まないので +1 
	}
	else if ( compare_head(iName, "isempty") && iName.size()>=8 ) {
		const char* p = iName.c_str()+7;
		mbinc(p);
		oResult = (*p=='\0') ? "1" : "0";
	}

	else if ( compare_head(iName, "文「") ) {
		if ( compare_tail(iName, "」の存在") ) {
			string	str(iName, 4, iName.length()-4-8);
			oResult = talks.is_exist(str) ? "1" : "0";
		}
		else if ( compare_tail(iName, "」の数") ) {
			string	str(iName, 4, iName.length()-4-6);

			Family<Talk>* f = talks.get_family(str);

			if ( f ) {
				oResult = itos(f->size_of_element());
			}
			else {
				oResult = "0";
			}
		}
	}
	else if ( compare_head(iName, "単語群「") ) {
		if ( compare_tail(iName, "」の存在") ) {
			string	str(iName, 8, iName.length()-8-8);
			oResult = words.is_exist(str) ? "1" : "0";
		}
		else if ( compare_tail(iName, "」の数") ) {
			string	str(iName, 8, iName.length()-8-6);

			int count = 0;

			Family<Word>* f = words.get_family(str);
			if ( f ) {
				count += f->size_of_element();
			}

			oResult = int2zen(count);
		}
		else if (compare_tail(iName, "」の重複回避枯渇")) {
			string str(iName, 8, iName.length() - 8 - 16);
			Family<Word>* f = words.get_family(str);
			bool is_used_all = false;
			if (f) {
				if (f->is_OC_used_all(*this)) {
					is_used_all = true;
				}
			}
			if (is_used_all){
				oResult = "1";
			}
			else {
				oResult = "0";
			}
		}
	}


	else if ( compare_head(iName, "変数「") && compare_tail(iName, "」の存在") ) {
		string	str(iName, 6, iName.length()-6-8);
		bool isSysValue;
		string *v = GetValue(str,isSysValue); //こっちはシステム変数かどうかどっちでもいい
		oResult = v ? "1" : "0";
	}
	else if ( compare_head(iName, "変数「") && compare_tail(iName, "」か０") ) {
		string	str(iName, 6, iName.length()-6-6);
		bool isSysValue;
		string *v = GetValue(str,isSysValue); //こっちはシステム変数かどうかどっちでもいい
		oResult = v ? *v : "０";
	}
	else if ( compare_head(iName, "変数「") && compare_tail(iName, "」か空文字列") ) {
		string	str(iName, 6, iName.length()-6-12);
		bool isSysValue;
		string *v = GetValue(str,isSysValue); //こっちはシステム変数かどうかどっちでもいい
		oResult = v ? *v : "";
	}


	else if (compare_head(iName, "導入済みゴースト「") && compare_tail(iName, "」の存在")){
		string	str(iName, 18, iName.length() - 18 - 8);
		oResult = installed_ghost_name.count(str) ? "1" : "0";
	}
	else if (compare_head(iName, "導入済みシェル「") && compare_tail(iName, "」の存在")){
		string	str(iName, 16, iName.length() - 16 - 8);
		oResult = installed_shell_name.count(str) ? "1" : "0";
	}
	else if (compare_head(iName, "導入済みバルーン「") && compare_tail(iName, "」の存在")){
		string	str(iName, 18, iName.length() - 18 - 8);
		oResult = installed_balloon_name.count(str) ? "1" : "0";
	}
	else if (compare_head(iName, "導入済みヘッドライセンサ「") && compare_tail(iName, "」の存在")){
		string	str(iName, 26, iName.length() - 26 - 8);
		oResult = installed_headline_name.count(str) ? "1" : "0";
	}
	else if (compare_head(iName, "導入済みフォント「") && compare_tail(iName, "」の存在")){
		string	str(iName, 18, iName.length() - 18 - 8);
		oResult = installed_font_name.count(str) ? "1" : "0";
	}
	else if (compare_head(iName, "導入済みプラグイン「") && compare_tail(iName, "」の存在")){
		string	str(iName, 20, iName.length() - 20 - 8);
		oResult = installed_plugin.count(str) ? "1" : "0";
	}
	else if (compare_head(iName, "導入済みプラグイン「") && compare_tail(iName, "」のID")){
		string	str(iName, 20, iName.length() - 20 - 6);
		if (installed_plugin.count(str)){
			oResult = installed_plugin[str].plugin_id;
		}
	}
	else if (compare_head(iName, "使ってるぞグラフ「") && compare_tail(iName, "」の本体側の名前")){
		string	str(iName, 18, iName.length() - 18 - 16);
		if (rate_of_use_graph.count(str)){
			oResult = rate_of_use_graph[str].sakura_name;
		}
	}
	else if (compare_head(iName, "使ってるぞグラフ「") && compare_tail(iName, "」の相方側の名前")){
		string	str(iName, 18, iName.length() - 18 - 16);
		if (rate_of_use_graph.count(str)){
			oResult = rate_of_use_graph[str].kero_name;
		}
	}
	else if (compare_head(iName, "使ってるぞグラフ「") && compare_tail(iName, "」の起動回数")){
		string	str(iName, 18, iName.length() - 18 - 12);
		if (rate_of_use_graph.count(str)){
			oResult = rate_of_use_graph[str].boot_count;
		}
	}
	else if (compare_head(iName, "使ってるぞグラフ「") && compare_tail(iName, "」の単純累計分")){
		string	str(iName, 18, iName.length() - 18 - 14);
		if (rate_of_use_graph.count(str)){
			oResult = rate_of_use_graph[str].boot_minutes;
		}
	}
	else if (compare_head(iName, "使ってるぞグラフ「") && compare_tail(iName, "」の起動割合")){
		string	str(iName, 18, iName.length() - 18 - 12);
		if (rate_of_use_graph.count(str)){
			oResult = rate_of_use_graph[str].boot_percent;
		}
	}
	else if (compare_head(iName, "使ってるぞグラフ「") && compare_tail(iName, "」の状態")){
		string	str(iName, 18, iName.length() - 18 - 8);
		if (rate_of_use_graph.count(str)){
			oResult = rate_of_use_graph[str].status;
		}
	}
	else if (compare_head(iName, "ウインドウ「") && compare_tail(iName, "」の存在")){
		string	str(iName, 12, iName.length() - 12 - 8);

		oResult = ul2zen(FindTopLevelWindow(str.c_str(),false));
	}
	else if (compare_head(iName, "「") && compare_tail(iName, "」を含むウインドウの存在")){
		string	str(iName, 2, iName.length() - 2 - 24);

		oResult = ul2zen(FindTopLevelWindow(str.c_str(),true));
	}
	else if (compare_head(iName, "プロセス「") && compare_tail(iName, "」の存在")){
		string	str(iName, 10, iName.length() - 10 - 8);

		oResult = ul2zen(FindProcessName(str.c_str(),false));
	}
	else if (compare_head(iName, "「") && compare_tail(iName, "」を含むプロセスの存在")){
		string	str(iName, 2, iName.length() - 2 - 22);

		oResult = ul2zen(FindProcessName(str.c_str(),true));
	}

	else if (compare_head(iName, "起動中ゴースト「") && compare_tail(iName, "」の存在")){
		string	str(iName, 16, iName.length() - 16 - 8);
		oResult = otherghostname.count(str) ? "1" : "0"; //自分自身はotherghostnameには含まない
	}

	else if ( compare_tail(iName, "の存在") ) {
		updateGhostsInfo();	// ゴースト情報を更新
		std::vector<strmap>::iterator i=ghosts_info.begin();
		for ( ; i!=ghosts_info.end() ; ++i )
			if ( compare_head(iName, (*i)["name"]) )
				break;
			else if ( compare_head(iName, (*i)["keroname"]) )
				break;
		oResult = ( i==ghosts_info.end() ) ? "0" : "1";
	}
	else if ( compare_tail(iName, "のサーフェス") ) {
		updateGhostsInfo();	// ゴースト情報を更新
		std::vector<strmap>::iterator i=ghosts_info.begin();
		for ( ; i!=ghosts_info.end() ; ++i )
			if ( compare_head(iName, (*i)["name"]) ) {
				oResult = (*i)["sakura.surface"];
				break;
			} else if ( compare_head(iName, (*i)["keroname"]) ) {
				oResult = (*i)["kero.surface"];
				break;
			}

		if ( i==ghosts_info.end() ) {
			oResult = "-1";
		}
	}
	else if ( compare_head(hankaku, "FMO") && hankaku.size()>4 ) { // FMO?head
		updateGhostsInfo();	// ゴースト情報を更新
		unsigned int digit = 3;
		while ( isdigit(hankaku[digit]) ) { ++digit; }

		if ( digit > 3 ) {
			unsigned int index = strtoul(hankaku.c_str()+3,NULL,10);
			if ( index < ghosts_info.size() ) {
				strmap&	m=ghosts_info[index];
				string	value(hankaku.c_str()+digit);
				if ( m.find(value) != m.end() ) {
					oResult = m[value];
				}
			}
		}
	}
	else if ( compare_head(hankaku, "count") )
	{
		string	name(hankaku.c_str()+5);
		int r = count_func(name);
		if ( r >= 0 ) {
			oResult = int2zen(r);
		}
	}
	else if ( iName=="セーブデータ読み込み" ) {
		oResult = load_savedata_status;
	}
	else if ( iName=="次のトーク" ) {
		std::map<int,string>::const_iterator it = reserved_talk.find(1);
		if ( it != reserved_talk.end() ) 
			oResult = it->second;
	}
	else if ( compare_head(iName,"次から") && compare_tail(iName,"回目のトーク") ) {
		int	count = zen2int( string(iName.c_str()+6, iName.length()-6-12) );
		std::map<int,string>::const_iterator it = reserved_talk.find(count);
		if ( it != reserved_talk.end() ) {
			oResult = it->second;
		}
	}
	else if ( compare_head(iName, "トーク「") && compare_tail(iName, "」の予\x96\xf1有無") ) { // 「約」には\が含まれる。
		string	str(iName, 8, iName.length()-8-12);
		oResult = "0";
		for (std::map<int, string>::const_iterator it=reserved_talk.begin(); it!=reserved_talk.end() ; ++it) {
			if ( str == it->second ) {
				oResult = "1";
				break;
			}
		}
	}
	else if ( iName == "予\x96\xf1トーク数" ) { // 「約」には\が含まれる。
		oResult = int2zen( reserved_talk.size() );
	}
	else if ( iName == "イベント名" ) { oResult=mRequestID; }
	else if ( iName == "直前の選択肢名" ) { oResult=last_choice_name; }
	else if ( hankaku == "pwd" ) { oResult=mBaseFolder; }
	else if ( iName == "本体の所在" ) { oResult=mExeFolder; }
	else if ( mRequestMap.find(iName) != mRequestMap.end() ) {
		oResult = mRequestMap[iName];
	}
	else if (iName == "全変数列挙"){
		if (fDebugMode && secure_flag) {
			oResult = "";
			for (strmap::iterator i = variables.begin(); i != variables.end(); i++){
				oResult += string("＄") + i->first + "\t" + i->second + "\\n";
			}
		}
	}
	else {
		// 見つからなかった。通常喋り？
		//括弧展開後にチェックするようになったのでここは無効化
		//speaked_speaker.insert(speaker);
		//chars_spoken += oResult.size();
		GetSender().sender() << "（" << iName << "） not found." << std::endl;
		return	false;
	}

	if ( stack_size_before_call != 0 && stack_size_before_call <= kakko_replace_history.size() ) {
		kakko_replace_history[stack_size_before_call-1].push_back(oResult);
	}
	GetSender().sender() << "（" << iName << "）→" << oResult << "" << std::endl;
	return	true;
}

//countコール・getaistate互換用
int Satori::count_func(const string &name)
{
	if ( name=="Words" ) { return words.size_of_family(); }
	else if ( name=="Variable" ) { return variables.size(); }
	else if ( name=="Anchor" ) { return anchors.size(); }
	else if ( name=="Talk" ) { return talks.size_of_element(); }
	else if ( name=="Word" ) { return words.size_of_element(); }
	else if ( name=="NoNameTalk" )
	{
		Family<Talk>* f = talks.get_family("");
		return ( f==0 ) ? 0 : f->size_of_element();
	}
	else if ( name=="EventTalk" )
	{
		int	n=0;
		for ( std::map< string, Family<Talk> >::const_iterator it = talks.compatible().begin() ; it != talks.compatible().end() ; ++it ) {
			if ( compare_head(it->first, "On") ) {
				n += it->second.size_of_element();
			}
		}
		return n;
	}
	else if ( name=="OtherTalk" )
	{
		int	n=0;
		for ( std::map< string, Family<Talk> >::const_iterator it = talks.compatible().begin() ; it != talks.compatible().end() ; ++it ) {
			if ( !compare_head(it->first, "On") && !it->first.empty() ) {
				n += it->second.size_of_element();
			}
		}
		return n;
	}
	else if ( name=="Line" )
	{
		int	n=0;
		for ( std::map< string, Family<Talk> >::const_iterator it = talks.compatible().begin() ; it != talks.compatible().end() ; ++it )
		{
			std::vector<const Talk*> v;
			it->second.get_elements_pointers(v);
			for ( std::vector<const Talk*>::const_iterator el_it = v.begin() ; el_it != v.end() ; ++el_it )
			{
				n += (*el_it)->size();
			}
		}
		for ( std::map< string, Family<Word> >::const_iterator it = words.compatible().begin() ; it != words.compatible().end() ; ++it )
		{
			n += it->second.size_of_element();
		}
		return n;
	}
	else if ( name=="Parenthesis" )
	{
		int	n=0;
		for ( std::map< string, Family<Talk> >::const_iterator it = talks.compatible().begin() ; it != talks.compatible().end() ; ++it )
		{
			std::vector<const Talk*> v;
			it->second.get_elements_pointers(v);
			for ( std::vector<const Talk*>::const_iterator el_it = v.begin() ; el_it != v.end() ; ++el_it )
			{
				for ( Talk::const_iterator tk_it = (*el_it)->begin() ; tk_it != (*el_it)->end() ; ++tk_it )
				{
					n += count(*tk_it, "（");
				}
			}
		}
		for ( std::map< string, Family<Word> >::const_iterator it = words.compatible().begin() ; it != words.compatible().end() ; ++it )
		{
			std::vector<const Word*> v;
			it->second.get_elements_pointers(v);
			for ( std::vector<const Word*>::const_iterator el_it = v.begin() ; el_it != v.end() ; ++el_it )
			{
				n += count(**el_it, "（");
			}
		}
		return n;
	}
	return -1;
}

//ウインドウ列挙
#ifndef POSIX
typedef struct EnumWindowsInfo
{
	const char* txt;
	char title[1024];
	bool isPartial;
	HWND hWnd;
} EnumWindowsInfo;

BOOL CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam)
{
	EnumWindowsInfo &inf = *reinterpret_cast<EnumWindowsInfo*>(lParam);

	::GetWindowText(hwnd,inf.title,sizeof(inf.title)-1);

	if ( inf.title[0] ) {
		if ( inf.isPartial ) {
			if ( strstri_hz(inf.title,inf.txt) ) {
				inf.hWnd = hwnd;
				return FALSE;
			}
		}
		else {
			if ( stricmp(inf.title,inf.txt) == 0 ) {
				inf.hWnd = hwnd;
				return FALSE;
			}
		}
	}
	
	return TRUE;
}
#endif

//ウインドウ探索　ウインドウ存在判定で使う
unsigned long Satori::FindTopLevelWindow(const char* txt,bool isPartial)
{
#ifdef POSIX
	return 0;
#else

	if ( ! txt || ! *txt ) { return 0; }

	EnumWindowsInfo inf;
	inf.txt = txt;
	inf.isPartial = isPartial;
	inf.hWnd = NULL;

	::EnumWindows(EnumWindowsProc,(LPARAM)&inf);

	return (unsigned long)inf.hWnd;
#endif
}

//プロセス探索　ウインドウ存在判定で使う
unsigned long Satori::FindProcessName(const char* txt,bool isPartial)
{
#ifdef POSIX
	return 0;
#else

	if ( ! txt || ! *txt ) { return 0; }

	HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);

	PROCESSENTRY32 pinfo = {0};
	pinfo.dwSize = sizeof(pinfo);

	DWORD pid = 0;

	if ( ::Process32First(hSnap,&pinfo) ) {
		do {
			const char *pName = strrchr(pinfo.szExeFile,'\\');
			if ( pName ) {
				pName += 1;
			}
			else {
				pName = pinfo.szExeFile;
			}

			if ( isPartial ) {
				if ( strstri_hz(pName,txt) ) {
					pid = pinfo.th32ProcessID;
					break;
				}
			}
			else {
				if ( stricmp(pName,txt) == 0 ) {
					pid = pinfo.th32ProcessID;
					break;
				}
			}
		} while ( ::Process32Next(hSnap,&pinfo) );
	}

	::CloseHandle(hSnap);

	return pid;

#endif
}
