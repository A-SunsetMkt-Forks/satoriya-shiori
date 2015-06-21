#include	"Thread.h"

//////////DEBUG/////////////////////////
#include "warning.h"
#ifdef _WINDOWS
#ifdef _DEBUG
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif
////////////////////////////////////////

bool	Thread::create(void* iParam, bool iSuspendedCreate) {
	assert(mThread==NULL);

	// �X���b�h�֐��̖{���̈����̓����o�ɃR�s�[���Ă����B
	mParam=iParam;
	
	// �X���b�h�쐬
	mThread=::CreateThread(
		NULL,				// �Z�L�����e�B����
		0,					// �����X�^�b�N�T�C�Y 0 �Ŏ���
		Thread::ThreadProc,	// �X���b�h�֐�
		(LPVOID)this,		// �X���b�h�֐��̈���
		iSuspendedCreate ? CREATE_SUSPENDED : 0,	// �����Ɏ��s���邩�ۂ�
		&mID );				// �X���b�hID�󂯎��

	return	(mThread!=NULL);
}

void	Thread::close() {
	if ( mThread == NULL )
		return;
	::CloseHandle(mThread);
	mThread = NULL;
}
