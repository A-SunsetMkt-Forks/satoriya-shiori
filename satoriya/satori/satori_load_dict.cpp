#include	"satori.h"

#include	<fstream>
#include	<cassert>
#include <algorithm>

#include	"../_/Utilities.h"
#include	"satori_load_dict.h"

#ifdef POSIX
#  include <iostream>
#  include "stltool.h"
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

struct satori_unit
{
	string typemark;
	string name;
	string condition;
	strvec body;
};
#ifdef _DEBUG
std::ostream& operator<<(std::ostream& o, const satori_unit& su)
{
	o << su.typemark << "/" << su.name << "/" << su.condition << std::endl;
	o << su.body;
	return o;
}
#endif

static void lines_to_units(
	const std::vector<string>& i_lines,
	const std::vector<string>& i_typemarks,
	const string& i_name_cond_delimiter,
	std::vector<satori_unit>& o_units)
{
	std::vector<string>::const_iterator line_it = i_lines.begin();
	for ( ; line_it != i_lines.end() ; ++line_it)
	{
		//cout << *line_it << std::endl;

		// �s����typemarks�̂����ꂩ���o�����Ă��邩�T��
		std::vector<string>::const_iterator mark_it = i_typemarks.begin();
		for ( ; mark_it != i_typemarks.end() ; ++mark_it) 
		{
			if ( line_it->compare(0, mark_it->size(), mark_it->c_str()) ==0 )
			{
				break;
			}
		}
		
		// �o�����Ă���w�b�_�A�����Ȃ��΃{�f�B
		if ( mark_it != i_typemarks.end() )
		{
			satori_unit unit;
			unit.typemark = *mark_it;
			
			const char* name = line_it->c_str() + mark_it->size();
			const char* delimiter = strstr_hz(name, i_name_cond_delimiter.c_str());
			if ( delimiter==NULL )
			{
				unit.name.assign(name);
			}
			else
			{
				unit.name.assign(name, delimiter);
				unit.condition.assign(delimiter + i_name_cond_delimiter.size());
			}

			o_units.push_back(unit);
		}
		else
		{
			if ( o_units.empty() ) 
			{
				// o_units����̂Ƃ��i�ŏ���typemarks�o���O�j�̓R�����g�ƌ��􂵁A�������Ȃ��B
			}
			else
			{
				// �Ō��o_units�Ƀ~��ǉ�
				o_units.back().body.push_back(*line_it);
			}
		}
	}

	// �eunit�����̋�s�����
	for ( std::vector<satori_unit>::iterator i = o_units.begin() ; i != o_units.end() ; ++i )
	{
		while (true)
		{
			strvec::iterator j = i->body.end();
			if ( j == i->body.begin() || (--j)->length()>0 )
			{
				break;
			}
			i->body.pop_back();
		}

		//GetSender().sender() << *i << std::endl;
	}
}


// �ɂ߂č��m����UTF-8�ȕ���������o
static bool is_utf8_dic(const strvec& in)
{
	unsigned int possible_utf8_count = 0;
	static char possible_3byte_table[] = "\x83\x84\x86\x88\x89\x8a\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9e\xa0\xbf"; //�������i�j���O�`�X�F�����Q

	for ( strvec::const_iterator fi=in.begin() ; fi!=in.end() ; ++fi )
	{
		const char* p = fi->c_str();

		const char* ps = strstr(p,"\xef\xbc");

		while ( ps ) {
			char c = ps[2]; //3�o�C�g��

			if ( strchr(possible_3byte_table,c) ) {
				possible_utf8_count += 1;
				if ( possible_utf8_count >= 10 ) {
					return true;
				}
			}

			ps = strstr(ps+3,"\xef\xbc");
		}
	}

	return false;
}

// �v���v���Z�X�I�ȏ����B
// ���s�L�����Z���K�p�A�R�����g�폜�Abefore_replace�̓K�p
static bool pre_process(
	const strvec& in,
	strvec& out,

	escaper& io_escaper,
	strmap& io_replace_dic
	
	)
{
	int	kakko_nest_count=0;	// "�i" �̃l�X�g���B1�ȏ�̏ꍇ�͉��s�𖳌�������B
	string	accumulater="";	// �s������ނ�[��
	int	line_number=1;
	for ( strvec::const_iterator fi=in.begin() ; fi!=in.end() ; ++fi, ++line_number )
	{
		const char* p=fi->c_str();
		bool	escape = false;

		// �J�b�R���̏ꍇ�A�s���̃^�u�͖�������B
		if ( kakko_nest_count>0 )
			while ( *p=='\t' )
				++p;

		// ��s�i�����s�j�ɑ΂��鏈��
		while ( *p!='\0' )
		{
			string	c=get_a_chr(p);	// �S�p���p��킸�ꕶ���擾�B

			if ( escape ) 
			{
				accumulater += (c=="��") ? c : io_escaper.insert(c);
				escape = false;
			}
			else 
			{
				if ( c=="��" ) 
				{
					escape = true;
					continue;
				}
				if ( c=="��" )
					break;	// �s�I��

				if ( c=="�i" )
					++kakko_nest_count;
				else if (  c=="�j" && kakko_nest_count>0 )
					--kakko_nest_count;

				accumulater += c;
			}
		}

		if ( escape )
		{
			continue;
		}

		if ( kakko_nest_count==0 )
		{
			// �u�����������K�p
			for ( strmap::iterator di=io_replace_dic.begin() ; di!=io_replace_dic.end() ; ++di )
			{
				replace(accumulater, di->first, di->second);
			}

			// ��s�ǉ�
			out.push_back(accumulater);
			//GetSender().sender() << line_number << " [" << accumulater << "]" << std::endl;
			accumulater="";
		}
		else if ( line_number == in.size() ) 
		{
			// �G���[
			return false;
		}
	}
	return true;
}

string	Satori::SentenceToSakuraScriptExec_with_PreProcess(const strvec& i_vec)
{
	strvec vec;
	pre_process(i_vec, vec, m_escaper, replace_before_dic);
	return SentenceToSakuraScriptExec(vec);
}

// .txt��.sat�̗���������̂ŁA�V������������ǂݍ��ށB
bool Satori::select_dict_and_load_to_vector(const string& iFileName, strvec& oFileBody, bool warnFileName)
{
	string txtfile = set_extention(iFileName, dic_load_ext);
	string satfile = set_extention(iFileName, "sat");

	string realext = get_extention(iFileName);

	bool FileExist(const string& f);
	bool decodeMe = false;
	string file;

	//SAT / TXT
	if ( realext == "sat" ) {
		if ( FileExist(satfile.c_str()) ) {
			file = satfile;
			decodeMe = true;
		}
		else {
			if ( warnFileName ) {
				GetSender().sender() << "  " << satfile << "is not exist." << std::endl;
			}
			file = txtfile;
		}
	}
	else {
		if ( FileExist(txtfile.c_str()) ) {
			file = txtfile;
		}
		else {
			if ( warnFileName ) {
				GetSender().sender() << "  " << txtfile << "is not exist." << std::endl;
			}
			file = satfile;
			decodeMe = true;
		}
	}

	GetSender().sender() << "  loading " << get_file_name(file);
	if ( !strvec_from_file(oFileBody, file) )
	{
		GetSender().sender() << "... failed.";
		return	false;
	}
	GetSender().sender() << std::endl;

	if ( decodeMe ) {
		// �Í���������
		for ( strvec::iterator it=oFileBody.begin() ; it!=oFileBody.end() ; ++it )
		{
			*it = decode( decode(*it) );
		}
	}

	return true;
}

static bool satori_anchor_compare(const string &lhs,const string &rhs)
{
	return lhs.size() > rhs.size();
}

// ������ǂݍ��ށB
bool Satori::LoadDictionary(const string& iFileName,bool warnFileName) 
{
	// �t�@�C������vector�֓ǂݍ��ށB
	// ���̍ہA���t�@�C�����Ŋg���q��.txt(�܂��͎w��g���q)��.sat�̃t�@�C���̓��t���r���A�V�������������̗p����B
	strvec	file_vec;
	if ( !select_dict_and_load_to_vector(iFileName, file_vec, warnFileName) )
	{
		return false;
	}

	if ( is_utf8_dic(file_vec) ) {
#ifdef POSIX
	     GetSender().errsender() <<
		    "syntax error - SATORI : " << iFileName << std::endl <<
		    std::endl <<
		    "It is highly possible that you tried to read a dictionary whose character code is UTF-8." << std::endl <<
		    "The dictionary is not loaded correctly." << std::endl <<
		    std::endl <<
		    "Please use Shift-JIS character code." << std::endl;
#else
		GetSender().errsender() << iFileName + "\n\n"
			"\n"
			"�����R�[�h��UTF-8�̎�����ǂݍ������Ƃ����\���������ł��B" "\n"
			"�����͐������ǂݍ��܂�Ă��܂���B" "\n"
			"\n"
			"�ۑ��̍ۂɁAShift-JIS���w�肵�Ă�������" << std::endl;
#endif
		return false;
	}

	bool	is_for_anchor = compare_head(get_file_name(iFileName), dic_load_prefix + "Anchor");

	strvec preprocessed_vec;
	if ( false == pre_process(file_vec, preprocessed_vec, m_escaper, replace_before_dic) )
	{
#ifdef POSIX
	     GetSender().errsender() <<
		    "syntax error - SATORI : " << iFileName << std::endl <<
		    std::endl <<
		    "There are some mismatched parenthesis." << std::endl <<
		    "The dictionary is not loaded correctly." << std::endl <<
		    std::endl <<
		    "If you want to display parenthesis independently," << std::endl <<
		    "use \"phi\" symbol to escape it." << std::endl;
#else
		GetSender().errsender() << iFileName + "\n\n"
			"\n"
			"�J�b�R�̑Ή��֌W���������Ȃ�����������܂��B" "\n"
			"�����͐������ǂݍ��܂�Ă��܂���B" "\n"
			"\n"
			"�J�b�R��P�Ƃŕ\������ꍇ�́@�Ӂi�@�ƋL�q���Ă��������B" << std::endl;
#endif
	}

	static std::vector<string> typemarks;
	if ( typemarks.empty() )
	{
		typemarks.push_back("��");
		typemarks.push_back("��");
	}

	std::vector<satori_unit> units;
	lines_to_units(preprocessed_vec, typemarks, "\t", units); // �P��Q��/�g�[�N���ƍ̗p�������̋�؂�


	for ( std::vector<satori_unit>::iterator i=units.begin() ; i!=units.end() ; ++i)
	{
		// �����̋�s���폜
		//while ( i->body.size()>0 && i->body.size()==0 )
		//{
		//	i->body.pop_back();
		//}
	        while (i->body.size() > 0 && i->body[i->body.size()-1].length() == 0) {
		        i->body.pop_back();
		}

		m_escaper.unescape(i->name);
		
		if ( i->typemark == "��" )
		{
			// �g�[�N�̏ꍇ
			if ( is_for_anchor ) {
				if ( i->name.size() > 0 ) {
					anchors.push_back(i->name);
				}
			}
			talks.add_element(i->name, i->body, i->condition);

#ifdef _DEBUG
			GetSender().sender() << "��" << i->name << " " << i->condition << std::endl;
#endif
		}
		else
		{
			// �P��Q�̏ꍇ
			const strvec& v = i->body;
			for ( strvec::const_iterator j=v.begin() ; j!=v.end() ; ++j)
			{
				words.add_element(i->name, *j, i->condition);
			}

#ifdef _DEBUG
			GetSender().sender() << "��" << i->name << " " << i->condition << std::endl;
#endif
		}

	}

	if ( is_for_anchor ) {
		std::sort(anchors.begin(),anchors.end(),satori_anchor_compare);
	}

	//GetSender().sender() << "�@�@�@talk:" << talks.count_all() << ", word:" << words.count_all() << std::endl;
	GetSender().sender() << "... ok." << std::endl;
	return	true;
}

#ifdef POSIX
#  include <sys/types.h>
#  include <dirent.h>
#endif

void list_files(string i_path, std::vector<string>& o_files)
{
	unify_dir_char(i_path); // \\��/�����ɉ����ēK�؂ȕ��ɓ���
#ifdef POSIX

	DIR* dh = opendir(i_path.c_str());
	if (dh == NULL)
	{
	    GetSender().sender() << "file not found." << std::endl;
	}
	while (1) {
	    struct dirent* ent = readdir(dh);
	    if (ent == NULL) {
		break;
	    }
#if defined(__WINDOWS__) || defined(__CYGWIN__)
	    string fname(ent->d_name);
#else
//	    string fname(ent->d_name, ent->d_namlen);
	    string fname(ent->d_name);
#endif
		o_files.push_back(fname);
	}
	closedir(dh);
#else /* POSIX */
	HANDLE			hFIND;	// �����n���h��
	WIN32_FIND_DATA	fdFOUND;// ���������t�@�C���̏��
	hFIND = ::FindFirstFile((i_path+"*.*").c_str(), &fdFOUND);
	if ( hFIND == INVALID_HANDLE_VALUE )
	{
		GetSender().sender() << "file not found." << std::endl;
	}

	do
	{
		o_files.push_back(fdFOUND.cFileName);
	} while ( ::FindNextFile(hFIND,&fdFOUND) );
	::FindClose(hFIND);
#endif /* POSIX */

}




int Satori::LoadDicFolder(const string& i_base_folder)
{
	GetSender().sender() << "LoadDicFolder(" << i_base_folder << ")" << std::endl;
	std::vector<string> files;
	list_files(i_base_folder, files);

	int count = 0;
	
	string ext = ".";
	ext += dic_load_ext;
	
	for (std::vector<string>::const_iterator it=files.begin() ; it!=files.end() ; ++it)
	{
		const int len = it->size();
		if ( len < 3 ) { continue; } // �ŒZ�t�@�C����3�����ȏ�
		if ( it->compare(0,3,dic_load_prefix.c_str()) != 0 ) { continue; }
		if ( it->compare(len-4,4,ext.c_str()) != 0 && it->compare(len-4,4,".sat") != 0 ) { continue; }

		if ( LoadDictionary(i_base_folder + *it) ) {
			++count;
		}
	}

	GetSender().sender() << "ok." << std::endl;
	return count;
}
