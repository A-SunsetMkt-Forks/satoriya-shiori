#include "Family.h"
#include "random.h"


// �v�f�͒P��܂��̓g�[�N�B

// �������O�����v�f�̏W����Family�B

// Families�͖��O�ɂ����肳���Family�̏W���B
// �ق� map< string, Family<T> > ����public�p���͂����A�C���^�t�F�[�X�����肷��

typedef enum {
	COMSEARCH_DEFAULT,
	COMSEARCH_LENGTH,
	COMSEARCH_TAG
} FamilyComSearchType;

template<typename T>
class Families
{
    typedef typename std::map< string, Family<T> >::iterator iterator;
    typedef typename std::map< string, Family<T> >::const_iterator const_iterator;
	
	std::set<string> m_clearOC_at_talk_end;
	std::map< string, Family<T> > m_elements;
	
public:
	//Families() { cout << "Families()" << endl; }
	//~Families() { cout << "~Families()" << endl; }
	
	// �v�f�̓o�^
	const T& add_element(const string& i_name, const T& i_t, const Condition& i_condition = Condition())
	{
		std::pair<iterator,bool> found = m_elements.insert(std::pair<string, Family<T> >(i_name,Family<T>()));
		//std::pair<iterator,bool> found = m_elements.insert(map< string, Family<T> >::value_type(i_name,Family<T>()));
		if ( found.second ) {
			found.first->second.set_namevec(i_name);
		}
		return found.first->second.add_element(i_t, i_condition);
	}
	
	// �ߋ��݊��̒�
	const std::map< string, Family<T> >& compatible() const
	{
		return m_elements;
	}
	
	// ���O����Family���擾
	Family<T>* get_family(string i_name)
	{
		iterator i = m_elements.find(i_name);
		return ( i == m_elements.end() ) ? NULL : &(i->second);
	}
	
	// ���O�̑��݂��m�F
	bool is_exist(const string& i_name) const
	{
		return m_elements.find(i_name) != m_elements.end();
	}
	
	// T���P�I�����A���̃|�C���^��Ԃ�
	const T* select(const string& i_name, Evalcator& i_evalcator)
	{
		iterator it = m_elements.find(i_name);
		if ( it == m_elements.end() ) {
			return NULL;
		}
		return it->second.select(i_evalcator);
	}

	// T�����ׂđI������
	template <typename Candidates>
	void select_all(const string& i_name, Evalcator& i_evalcator,Candidates &candidates)
	{
		iterator it = m_elements.find(i_name);
		if ( it == m_elements.end() ) {
			return;
		}
		it->second.select_all(i_evalcator,candidates);
	}
	
	// �폜
	void erase(const string& i_name)
	{
		m_elements.erase(i_name);
		m_clearOC_at_talk_end.erase(i_name);
	}
	
	// �g�[�N�̏I����ʒm�B�d��������Ԃ��u�g�[�N���v�ł���Family�̏d����𐧌���N���A����
	void handle_talk_end()
	{
		for ( std::set<string>::iterator it = m_clearOC_at_talk_end.begin() ; it != m_clearOC_at_talk_end.end() ; ++it )
		{
			get_family(*it)->clear_OC();
		}
	}
	
	// family��
	int size_of_family() const
	{
		return m_elements.size();
	}
	
	// �SFamily�̑S�v�f�����v�Z
	int size_of_element() const
	{
		int r = 0;
		for ( const_iterator it = m_elements.begin() ; it != m_elements.end() ; ++it )
		{
			r += it->second.size_of_element();
		}
		return r;
	}
	
	// �S�N���A
	void clear()
	{
		m_elements.clear();
		m_clearOC_at_talk_end.clear();
	}
	
	// �d����𐧌��I������B�����̓^�C�v�A����
	void setOC(string i_name, string i_value)
	{
		iterator st, ed;
		if ( i_name == "��" )
		{
			st = m_elements.begin();
			ed = m_elements.end();
		}
		else
		{
			st = m_elements.find(i_name);
			if ( st == m_elements.end() )
			{
				GetSender().sender() << "'" << i_name << "' �͑��݂��܂���B" << std::endl;
				return;
			}
			++(ed = st);
		}
		
		//�����A�N�����@�ȂǕ����w��̏ꍇ������
		strvec argv;
		const int n = split(i_value, "�A,", argv);
		const string method = (argv.size()>=1) ? argv[0] : "����";
		const string span = (argv.size()>=2) ? argv[1] : "�N����";
		
		for ( iterator it = st; it != ed ; ++it )
		{
			Family<T>& family = it->second;
			if ( family.empty() )
			{
				continue;
			}
			
			if ( method=="���O" )
				family.set_OC(new OC_NonDual<const T*>);
			else if ( method=="�~��" || method=="����" )
				family.set_OC(new OC_Sequential<const T*>);
			else if ( method=="����" || method=="�t��" )
				family.set_OC(new OC_SequentialDesc<const T*>);
			else if ( method=="�L��" || method=="���S" )
				family.set_OC(new OC_NonOverlap<const T*>);
			else if ( method=="����" )
				family.set_OC(new OC_Random<const T*>);
			else
				GetSender().sender() << "�d����𐧌�̕��@'" << method << "' �͒�`����Ă��܂���B" << std::endl;
			
			if ( span == "�g�[�N��" )
				m_clearOC_at_talk_end.insert(it->first);
			else if ( span == "�N����")
				m_clearOC_at_talk_end.erase(it->first);
			else
				GetSender().sender() << "�d������̊���'" << method << "' �͒�`����Ă��܂���B" << std::endl;
			
		}
	}

	bool isOCUsedAll(const string& i_name)
	{
		iterator st, ed;
		if (i_name == "��")
		{
			//�S�Ώۂɂ͎g���Ȃ�
			return false;
		}
		else
		{
			st = m_elements.find(i_name);
			if (st == m_elements.end())
			{
				GetSender().sender() << "'" << i_name << "' �͑��݂��܂���B" << endl;
				return false;
			}
			Family<T>& family = st->second;
			return family.is_OC_used_all();
		}
	}
	
	const Talk* communicate_search(const string& iSentence, bool iAndMode, FamilyComSearchType type, Evalcator& i_evalcator)
	{
		GetSender().sender() << "�����̌������J�n" << std::endl;
		GetSender().sender() << "�@�Ώە�����: " << iSentence << std::endl;
		GetSender().sender() << "�@�S�P���v���[�h: " << (iAndMode?"true":"false") << std::endl;

		std::vector<iterator> elem_vector;

		std::string::size_type sentenceNamePos = find_hz(iSentence,"�u");

		bool isComNameMode = sentenceNamePos != string::npos;
		if ( isComNameMode ) {
			GetSender().sender() << "�@�u�����A���O���胂�[�h�Ɉڍs" << std::endl;
			for ( iterator it = m_elements.begin() ; it != m_elements.end() ; ++it )
			{
				if ( it->second.is_comname() ) {
					string comName = it->second.get_comname();
					if ( comName.length() ) {
						if ( comName == "�u" ) { //�Ȃ�ł�������L�@
							elem_vector.push_back(it);
						}
						if ( iSentence.compare(0,comName.size(),comName) == 0 ) {
							elem_vector.push_back(it);
						}
					}
				}
			}
		}
		else {
			GetSender().sender() << "�@�u�Ȃ��A�ʏ�R�~���T�����[�h�Ɉڍs" << std::endl;
			for ( iterator it = m_elements.begin() ; it != m_elements.end() ; ++it )
			{
				if ( ! it->second.is_comname() ) {
					elem_vector.push_back(it);
				}
			}
			sentenceNamePos = 0;
		}

		if ( elem_vector.size() <= 0 ) {
			GetSender().sender() << "����: �Y���Ȃ��i�����������Ȃ��j" << std::endl;
			return	NULL;
		}
		
		std::vector<iterator> hit_vector;
		int	max_hit_point=0;
		for ( typename std::vector<iterator>::iterator it = elem_vector.begin() ; it != elem_vector.end() ; ++it )
		{
			// ��Q��S�p�X�y�[�X�ŋ�؂�
			const strvec &words = (**it).second.get_namevec();
			
			// �����̒P�ꂪ�q�b�g�������B�P��P��10�_�{���A�����P������1�_�B��v�������������񂪒����قǃ{�[�i�X����B
			int	hit_point=0;
			strvec::const_iterator wds_it=words.begin();
			if ( isComNameMode ) { wds_it += 1; }; //�ЂƂ߂͖��O�i���łɒ��o�ρj

			for ( ; wds_it!=words.end() ; ++wds_it )
			{
				bool test = false;
				if (type == COMSEARCH_TAG) {
					std::string s = iSentence.substr(sentenceNamePos + 4);	//+4�͋󔒂ƃJ�b�R��
					test = s == *wds_it;
				}
				else {
					test = find_hz(iSentence, *wds_it, sentenceNamePos) != std::string::npos;
				}

				if ( test )
				{
					if ( (!isComNameMode) && compare_tail(*wds_it, "�u") ) { // ������ �u �ł�����̂����̏ꍇ�̓q�b�g�ƌ��Ȃ��Ȃ��悤�ɁB
						hit_point += 4;
					}
					else {
						//�P���v�B�_���v�Z�B
						if ( type == COMSEARCH_LENGTH ) {
							hit_point += 10*wds_it->size();
						}
						else if (type == COMSEARCH_TAG){
							hit_point = 100;
						}
						else {
							hit_point += 10+(wds_it->size()/4);
						}
					}
				}
				else
				{
					if (type != COMSEARCH_LENGTH && type != COMSEARCH_TAG)//�^�O�����E���v�������ŃR�~���j�P�[�g�q�b�g���Z�o����ꍇ�A���_���������
					{
						hit_point -= (iAndMode ? 999 : 1);	// ��v���Ȃ������A������Ȃ������P��
					}
				}
			}
			if ( hit_point<=4 )
			{
				continue;	// ����������v���Ȃ��ꍇ
			}
			
			GetSender().sender() << "'" << (**it).first << "' : " << hit_point << "pt ,";
			
			if (type == COMSEARCH_TAG)
			{
				if (hit_point <= 0)
				{
					continue;
				}
			}
			else
			{
				if (hit_point < max_hit_point) {
					GetSender().sender() << "�p��" << std::endl;
					continue;
				}
				else if (hit_point == max_hit_point) {
					GetSender().sender() << "���Ƃ��Ēǉ�" << std::endl;
				}
				else {
					max_hit_point = hit_point;
					GetSender().sender() << "�P�Ƃō̗p" << std::endl;
					hit_vector.clear();
				}
			}
			
			hit_vector.push_back(*it);
		}

		//�d������̃`�F�b�N
		std::vector<const Talk*>	result;
		bool is_remain_talk = false;
		for (typename std::vector<iterator>::iterator it = hit_vector.begin(); it != hit_vector.end(); ++it)
		{
			//�I����₪�c���Ă��邩�m�F
			if (!(**it).second.is_OC_used_all(i_evalcator))
			{
				//�̂����Ă�����̂��P�ł������OK
				is_remain_talk = true;
			}
		}

		//��⃉���_�����X�g�̍쐬
		for (typename std::vector<iterator>::iterator it = hit_vector.begin(); it != hit_vector.end(); ++it)
		{
			if (!is_remain_talk)
			{
				//�I�ׂ���̂��Ȃ��̂Ń��Z�b�g
				(**it).second.clear_OC();
			}
			(**it).second.get_elements_pointers_selectables(result, i_evalcator);
		}

		
		GetSender().sender() << "����: ";
		if ( result.size() <= 0 ) {
			GetSender().sender() << "�Y���Ȃ��i������₠��A�P�ꌟ�����s�j" << std::endl;
			return	NULL;
		}

		const Talk* res = result[ random(result.size()) ];

		//�I���������̂��d������ɓn������
		//�O���I���ɂȂ��Ă���̂ŗ�O�I�c
		for (typename std::vector<iterator>::iterator it = elem_vector.begin(); it != elem_vector.end(); ++it)
		{
			(**it).second.applySelectedOC(i_evalcator, res);
		}
		
		return res;
	}
};



