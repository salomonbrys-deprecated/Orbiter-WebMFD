// ==============================================================
//                  ORBITER MODULE: WebMFD
//            Copyright (C) 2010 Salomon BRYS
//                   All rights reserved
//
// ServerMFD.h
//
// Class interface for ServerMFD. Handles the MFDs displayed by
// the webserver.
// This code is inspired by ExtMFD module by Martin Schweiger
// ==============================================================

#ifndef __SERVERMFD_H
#define __SERVERMFD_H

#include <Orbitersdk.h>
#include <atlimage.h>

struct CppMFDSPEC : public MFDSPEC
{
	CppMFDSPEC(LONG pos_left, LONG pos_top, LONG pos_right, LONG pos_bottom,
		int nbt_left, int nbt_right, int bt_yofs, int bt_ydist)
	{
		this->pos.left = pos_left;
		this->pos.top = pos_top;
		this->pos.right = pos_right;
		this->pos.bottom = pos_bottom;
		this->nbt_left = nbt_left;
		this->nbt_right = nbt_right;
		this->bt_yofs = bt_yofs;
		this->bt_ydist = bt_ydist;
	}
};

class ServerMFD : public ExternMFD
{
public:
	ServerMFD();
	
	int				Width()  const { return _spec.pos.right; }
	int				Height() const { return _spec.pos.bottom; }
	//std::string		TempFileName() const { return _tempFileName; }

	void			Register() { oapiRegisterExternMFD(this, _spec); _generateJSON(); }
	void			unRegister() { oapiUnregisterExternMFD(this); }

	virtual void	clbkRefreshDisplay(SURFHANDLE);

	void			addPng() { ++_png; }
	void			remPng() { if (_png > 0) --_png; }
	void			addJpeg() { ++_jpeg; }
	void			remJpeg() { if (_jpeg > 0) --_jpeg; }
	void			addNox() { ++_nox; }
	void			remNox() { if (_nox > 0) --_nox; }
	unsigned int	Followers() { return _png + _jpeg + _nox; }

	HANDLE			getFileIf(const std::string &format, unsigned int &prevId);
	void			closeFile(HANDLE file);

	void			waitForBtnProcess();
	void			startBtnProcess();
	void			endBtnProcess(int btnId);
	std::string		getJSON();
	bool			getClose();

protected:
	virtual ~ServerMFD(void);

private:
	void			_generateJSON();

	CppMFDSPEC		_spec;
	std::string		_tempFileName;
	HANDLE			_fileMutex;
	unsigned int	_fileId;

	unsigned int	_png;
	unsigned int	_jpeg;
	unsigned int	_nox;

	HANDLE		_btnMutex;
	bool		_btnProcess;
	bool		_btnClose;

	std::string _JSON;
};

#endif // __SERVERMFD_H
