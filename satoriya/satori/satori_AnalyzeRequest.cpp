#include	"satori.h"

#ifdef POSIX
/* POSIX�ł�stricmp�͒�`����Ă��Ȃ��B�����strcasecmp���g����B */
#  define stricmp strcasecmp
#  include <string.h>
#endif

//////////DEBUG/////////////////////////
#include "warning.h"
#ifdef _WINDOWS
#ifdef _DEBUG
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif
////////////////////////////////////////

int	Satori::request(
	const string& i_protocol,
	const string& i_protocol_version,
	const string& i_command,
	const strpairvec& i_data,
	
	string& o_protocol,
	string& o_protocol_version,
	strpairvec& o_data)
{
	SenderEnableBuffering seb(GetSender());

	//-------------------------------------------------
	// ���N�G�X�g�P�ʂ̃N���X�����o��������

	mRequestMap.clear();
	mRequestID = "";
	mReferences.clear();
	mReferences.reserve(8); // �ŏ��l

	// �������N���X�����o�ɐݒ�
	mRequestCommand = i_command;
	mRequestType = i_protocol;
	mRequestVersion = i_protocol_version;
	mStatusLine = i_command + " " + i_protocol + "/" + i_protocol_version;

	// �Ԃ��v���g�R���̃f�t�H���g�l
	o_protocol = "SHIORI";
	o_protocol_version = "3.0";

	// ���邲�Ƃɏ���������ϐ�
	return_empty = false;

	surface_restore_at_talk_onetime = SR_INVALID;
	auto_anchor_enable_onetime = auto_anchor_enable;
	auto_newline_enable_onetime = auto_newline_enable;

	is_quick_section = false;

	// �X�N���v�g�w�b�_
	header_script = "";

	// �v���g�R���𔻕�
	if ( i_protocol=="SAORI" && i_protocol_version[0]>='1' )
	{
		o_protocol = i_protocol;
		o_protocol_version = "1.0";
		mRequestMode = SAORI;
	}
	else if ( i_protocol=="SHIORI" && i_protocol_version[0]>='3' )
	{
		mRequestMode = SHIORI3;
	}
	else if ( i_protocol=="SHIORI" && i_protocol_version[0]=='2' )
	{
		mRequestMode = SHIORI2;
			// 2.x�ɂ��o�[�W�����Ƃ��Ă�3.0��Ԃ�
	}
	else if ( i_protocol=="MAKOTO" && i_protocol_version[0]>='2' )
	{
		o_protocol = i_protocol;
		o_protocol_version = "2.0";
		mRequestMode = MAKOTO2;
	}
	else if ( i_protocol=="UNKNOWN" )
	{
		mRequestMode = UNKNOWN;
	}
	else
	{
		// ���Ή��̃v���g�R���������B
		return	400;
	}

	// �f�[�^����mRequestMap�Ɋi�[�B
	// SHOIRI/3.0�ȊO�̃v���g�R���̏ꍇ�ASHOIRI/3.0�ɕϊ����s���B
	for ( strpairvec::const_iterator it = i_data.begin() ; it != i_data.end() ; ++it )
	{
		string key = it->first;
		const string& value = it->second;

		switch ( mRequestMode ) {
		case SAORI:
			if ( compare_head(key, "Argument") ) {
				int	n = stoi_internal(key.c_str()+8);
				if ( n==0 )
					key = "ID";
				else
					key = "Reference" + itos(n-1);
			}
			break;
		case SHIORI2:	// �������͂Ă��Ɓ[
			if ( key=="Event" )
				key="ID";
			break;
		case MAKOTO2:
			if ( key=="String" )
				key="Reference0";
			break;
		default:
			break;
		}

		mRequestMap[key] = value;
		if ( compare_head(key, "Reference") ) {
			int	n = stoi_internal(key.c_str()+9);
			if ( n>=0 && n<65536 ) {
				if ( n>=mReferences.size() )
					mReferences.resize(n+1);
				mReferences[n]=value;
			}
		}
	}

	if ( mRequestMode == MAKOTO2 )
	{
		mRequestMap["ID"] = "OnMakoto";
	}

	mRequestID = mRequestMap["ID"];
	mIsMateria = ( mRequestMap["Sender"] == "embryo" );
	mIsStatusHeaderExist = ( mRequestMap["Sender"] == "SSP" );

	//-------------------------------------------------
	// ���N�G�X�g������

	if ( mRequestCommand=="GET Version" )
	{
		if ( mRequestMode == SHIORI2 )
		{
			o_data.push_back( strpair("ID", gSatoriName) );
			o_data.push_back( strpair("Craftman", gSatoriCraftman) );
			o_data.push_back( strpair("Version", gSatoriVersion) );
		}
		return 200;
	}


	// �I�𕪊�L�^��ϐ��ɑ���Bref0�����ɖ߂��B
	if ( compare_head(mRequestID, "OnChoice") ) // OnChoiceSelect/OnChoiceEnter�̗���
	{
		strvec	vec;
		int	ref_no = ( mRequestID=="OnChoiceEnter" || mRequestID=="OnChoiceSelectEx" )?1:0;
		string&	info = mRequestMap[string("Reference")+itos(ref_no)];
		if ( split(info, byte1_dlmt, vec)==3 ) // \1��؂�̂R������ł���Ȃ��
		{
			info=mReferences[ref_no]=variables["�I���h�c"]=vec[0];
			variables["�I�����x��"]=vec[1];
			variables["�I��ԍ�"]=vec[2];
		}
	}

	// ���O�ɂ��ĐF�X
	bool log_disable_soft = ( mRequestID=="OnSurfaceChange" || mRequestID=="OnSecondChange" || mRequestID=="OnMinuteChange"
		|| mRequestID=="OnMouseMove" || mRequestID=="OnTranslate");

	bool log_disable_hard = ( /*compare_tail(mRequestID, "caption") || */compare_tail(mRequestID, "visible")
		|| compare_head(mRequestID, "menu.") || mRequestID.find(".color.")!=string::npos );

	GetSender().next_event();

	if(fRequestLog)
	{
		GetSender().sender() << "--- Request ---" << std::endl << mStatusLine <<std::endl; // << iRequest <<std::endl;
		GetSender().sender() << "ID: " << mRequestID << std::endl;
		for (size_t i = 0; i < mReferences.size(); i++) {
			GetSender().sender() << "Reference" << i << ": " << mReferences[i] << std::endl;
		}
	}

	// �����゠�H
	strmap::const_iterator it = mRequestMap.find("SecurityLevel");
	secure_flag = ( it!=mRequestMap.end() && stricmp(it->second.c_str(), "local")==0 );

	// �\�ߎw�肵���C�x���g�v���t�B�b�N�X��external�ł����s�\��
	bool is_external = (it != mRequestMap.end() && stricmp(it->second.c_str(), "external") == 0);
	if (is_external) {
		for (auto prefix : allow_external_event_prefixes) {
			if (prefix == "�S��" || strstr(mRequestID.c_str(), prefix.c_str()) == mRequestID.c_str()) {
				secure_flag = true;
				break;
			}
		}
	}

	// ���C������
	GetSender().sender() << "--- Operation ---" << std::endl;

	int status_code = 500;
	if ( mRequestID=="enable_log" || mRequestID=="enable_debug" ) {
		if ( secure_flag ) {
			bool flag = false;
			if ( mReferences.size() > 0 ) {
				flag = atoi(mReferences[0].c_str()) != 0;
			}

			if ( mRequestID=="enable_debug" ) {
				fDebugMode = flag;
			}
			else {
				GetSender().validate(flag);
			}
			status_code = 200;
		}
		else {
			GetSender().sender() << "local/Local�łȂ��̂ŏR��܂���: ShioriEcho" <<std::endl;
			status_code = 403;
		}
	}
	else if ( mRequestID=="ShioriEcho" ) {
		// ShioriEcho����
		if ( fDebugMode && secure_flag ) {
			string result = SentenceToSakuraScriptExec_with_PreProcess(mReferences);
			if ( result.length() ) {
				static const char* const dangerous_tag[] = {"\\![updatebymyself]",
					"\\![vanishbymyself]",
					"\\![enter,passivemode]",
					"\\![enter,inductionmode]",
					"\\![leave,passivemode]",
					"\\![leave,inductionmode]",
					"\\![lock,repaint]",
					"\\![unlock,repaint]",
					"\\![biff]",
					"\\![open,browser",
					"\\![open,mailer",
					"\\![raise",
					"\\j["};

				std::string replace_to;
				for ( int i = 0 ; i < (sizeof(dangerous_tag)/sizeof(dangerous_tag[0])) ; ++i ) {
					replace_to = "���m";
					replace_to += dangerous_tag[i]+2; //\���k�L
					replace(result,dangerous_tag[i],replace_to);
				}

				//Translate(result); - Translate�͌�ł�����
				mResponseMap["Value"] = result;
				status_code = 200;
			}
			else {
				status_code = 204;
			}
		}
		else {
			if ( fDebugMode ) {
				GetSender().sender() << "local/Local�łȂ��̂ŏR��܂���: ShioriEcho" <<std::endl;
				status_code = 403;
			}
			else {
				static const std::string dbgmsg = "�f�o�b�O���[�h�������ł��B�g�p���邽�߂ɂ́��f�o�b�O���L���ɂ��Ă��������B: ShioriEcho";
				GetSender().sender() << dbgmsg <<std::endl;

				mResponseMap["Value"] = "\\0" + dbgmsg + "\\e";
				status_code = 200;
			}
		}
	}
	else if (mRequestID == "SatolistEcho"){
#ifndef POSIX
		// ���Ƃ肷�ƃf�o�b�K����
		if (fDebugMode && secure_flag) {

			//R0�͏��������
			strvec customRef;
			for (int i = 1; i < mReferences.size(); i++){
				customRef.push_back(mReferences[i]);
			}

			string result = SentenceToSakuraScriptExec_with_PreProcess(customRef);
			if (result.length()) {
				result = string("SSTP 200 OK\r\nCharset: Shift_JIS\r\nResult: ") + result + "\r\n\r\n";
			}
			else{
				//���Ȃ�
				result = string("SSTP 204 No Content\r\nCharset: Shift_JIS\r\n\r\n");
			}

			char* copyData = new char[result.length() + 1];
			strcpy(copyData, result.c_str());

			COPYDATASTRUCT cds;
			cds.dwData = 0;
			cds.cbData = result.length() + 1;
			cds.lpData = copyData;
			DWORD ret;

			SendMessageTimeout((HWND)stoi_internal(mReferences[0]), WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds, 0, 1000, &ret);

			//������204��Ԃ��Ɣ�Ή����̃G���[���o���̂�200�Œʒm���b�Z�[�W��\������
			status_code = 200;
			mResponseMap["Value"] = "\\0\\_q�����𑗐M���܂����B\\e";
		}
		else {
			if (fDebugMode) {
				GetSender().sender() << "local/Local�łȂ��̂ŏR��܂���: SatolistEcho" <<std::endl;
				status_code = 403;
			}
			else {
				static const std::string dbgmsg = "�f�o�b�O���[�h�������ł��B�g�p���邽�߂ɂ́��f�o�b�O���L���ɂ��Ă��������B: SatolistEcho";
				GetSender().sender() << dbgmsg <<std::endl;

				mResponseMap["Value"] = "\\0" + dbgmsg + "\\e";
				status_code = 200;
			}
		}
#endif // POSIX
	}
	else {
		status_code = CreateResponse(mResponseMap);
	}

	// �㏈���P
	default_surface = next_default_surface;

	//--------------------------------------------------------------------

	// Value�ɑ΂���ŏI����
	if ( status_code==200 ) {	// && compare_head(mRequestID, "On")
		strmap::iterator i = mResponseMap.find("Value");
		if ( i!=mResponseMap.end() ) {
			if ( return_empty ) {
				status_code = 204;
				mResponseMap.erase(i);
			}
			else {
				if ( !Translate(i->second) ) {
					status_code = 204;
					mResponseMap.erase(i);
				} 
				else {
					second_from_last_talk = 0;
					if ( compare_head(mRequestID, "On") ) {
						mResponseHistory.push_front(i->second);
						if ( mResponseHistory.size() >= RESPONSE_HISTORY_SIZE )
							mResponseHistory.pop_back();
					}
				}
			}
		}
	}

	GetSender().sender() << "status code : " << itos(status_code) <<std::endl;

	//--------------------------------------------------------------------

	for(strmap::const_iterator i=mResponseMap.begin() ; i!=mResponseMap.end() ; ++i)
	{
		string	key=i->first, value=i->second;

		switch ( mRequestMode ) {
		case SAORI:
			if ( key=="Value" ) {
				key = "Result";
				value = header_script + value;
			}
			else if ( compare_head(key, "Reference") ) {
				key = string() + "Value" + (key.c_str()+9);
			}
			break;
		case SHIORI2:
			if ( key=="Value" ) {
				key = "Sentence";
				value = header_script + value;
			}
			break;
		case MAKOTO2:
			if ( key=="Value" ) {
				key = "String";
				value = header_script + value;
			}
			break;
		default:
			if ( key=="Value" ) {
				value = header_script + value;
			}
			break;
		}
		o_data.push_back( strpair(key, value) );
	}

	if ( GetSender().errsender().get_log_mode() ) {
		const std::vector<string> &errlog = GetSender().errsender().get_log();

		std::string errmsg;
		std::string errlevel;

		for ( std::vector<string>::const_iterator itr = errlog.begin() ; itr != errlog.end(); ++itr ) {
			errmsg += "SATORI : ";
			errmsg += *itr;
			errmsg += "\1";
			errlevel += "critical\1";
		}

		if ( errmsg.length() ) {
			errmsg.erase(errmsg.end()-1,errmsg.end());
			errlevel.erase(errlevel.end()-1,errlevel.end());

			o_data.push_back( strpair("ErrorLevel",errlevel) );
			o_data.push_back( strpair("ErrorDescription",errmsg) );
		}
		GetSender().errsender().clear_log();
	}

	GetSender().validate();
	if(fResponseLog)
	{
		GetSender().sender() << "--- Response ---" <<std::endl << mResponseMap <<std::endl;
	}
	mResponseMap.clear();

	if ( log_disable_hard ) {
		GetSender().delete_last_request();
	}
	else if ( log_disable_soft ) {
		if ( status_code != 200 ) {
			GetSender().delete_last_request();
		}
	}

	GetSender().flush();

	//--------------------------------------------------------------------

	// �����[�h����
	if ( reload_flag )
	{
		reload_flag = false;
		string	tmp = mBaseFolder;
		GetSender().sender() << "����reloading." <<std::endl;
		unload();
		load(tmp);
		GetSender().sender() << "����reloaded." <<std::endl;

		GetSender().flush();
	}

	return	status_code;
}
