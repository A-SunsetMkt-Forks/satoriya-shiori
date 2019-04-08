#pragma once

#include "SakuraDLLClient.h"

// SAORI/1.0 -universal �N���C�A���g
//  �u�e�x����Ăяo���鋤�ʃv���O�C���K�i�Ή�DLL�v���ĂԁB
class SaoriClient : public SakuraDLLClient
{
public:
	SaoriClient() {}
	virtual ~SaoriClient() {}

	virtual bool load(
		const string& i_sender,
		const string& i_charset,
		const string& i_work_folder,
		const string& i_dll_fullpath);

	virtual int request(
		const std::vector<string>& i_argument,
		bool i_is_secure,
		string& o_result,
		std::vector<string>& o_value);
};

// �ł�SakuraDLLClient����p������͕̂s�K�؂�
// request���������o���ׂ���


