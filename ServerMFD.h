/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#ifndef __SERVERMFD_H
#define __SERVERMFD_H

#include <Orbitersdk.h>
#include <atlimage.h>

/// C++ version of Orbiter's MFDSPEC
/// Adds a constructor that initializes the data
struct CppMFDSPEC : public MFDSPEC
{
	/// Constructor
	/// All params are coresponding to the MFDSPEC fields
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

/// Image stream structure to be passed to image followers
struct imageStream
{
	/// Constructor
	imageStream() : Memory(0), stream(0), size(0) {}

	/// Memory handle for the stream
	HGLOBAL		Memory;

	/// Stream on which encoded images are buffered
	IStream *	stream;

	/// Size of the current image
	int			size;
};


/// Handles a MFD life cycle displayed by the web server
/// To be used by the WebMFD Server
class ServerMFD : public ExternMFD
{
public:
	/// Constructor
	ServerMFD();

	/// Gets the MFD Width
	/// FIXME: Currently, always 255
	/// \return The width of the MFD
	int				Width()  const { return _spec.pos.right; }

	/// Gets the MFD Height
	/// FIXME: Currently, always 255
	/// \return The height of the MFD
	int				Height() const { return _spec.pos.bottom; }

	/// Registers the MFD into the Orbiter simulation
	void			Register() { oapiRegisterExternMFD(this, _spec); _generateJSON(); }

	/// Unregisters the MFD into the Orbiter simulation
	void			unRegister() { oapiUnregisterExternMFD(this); }

	/// Callback called by Orbiter when the MFD should refresh
	/// \param[in]	hSurf	The surface containing the new MFD image
	virtual void	clbkRefreshDisplay(SURFHANDLE hSurf);

	/// Callback called by Orbiter when the buttons change
	virtual void clbkRefreshButtons();

	/// Informs the ServerMFD that there is one more follower that will be looking at its PNG image
	void			addPng() { ++_pngFollowers; }

	/// Informs the ServerMFD that there is one less follower that will be looking at its PNG image
	void			remPng() { if (_pngFollowers > 0) --_pngFollowers; }

	/// Informs the ServerMFD that there is one more follower that will be looking at its JPEG image
	void			addJpeg() { ++_jpegFollowers; }

	/// Informs the ServerMFD that there is one less follower that will be looking at its JPEG image
	void			remJpeg() { if (_jpegFollowers > 0) --_jpegFollowers; }

	/// Informs the ServerMFD that there is one more follower that won't be looking at any of it's image (will only use buttons)
	void			addNox() { ++_noxFollowers; }

	/// Informs the ServerMFD that there is one less follower that won't be looking at any of it's image
	void			remNox() { if (_noxFollowers > 0) --_noxFollowers; }

	/// Gets the total number of all folowers
	/// \return the number of folowers
	unsigned int	Followers() { return _pngFollowers + _jpegFollowers + _noxFollowers; }

	/// Returns a IStream* containing the desired image if and only if the prevId is not the current image id.
	/// The current id can never be 0, so a 0 prevId means that it should always return the stream, except when the MFD has been created but not yet refreshed.
	/// This will return 0 if:
	///   - The image is requested in a format that does not have any followers.
	///   - prevId is the same as the current image id.
	/// When the image is correctly returned:
	///   - prevId is updated to the current image id.
	///   - The ownership of the stream is granted, which means that the stream cannot be read or updated by any other thread until closeStream is called.
	/// \param[in]		format	The format of the image requested: "png" or "jpeg".
	/// \param[in,out]	prevId	The id of the previous image. Updated if there is a new image.
	/// \return the Istream or 0
	imageStream *	getStreamIf(const std::string &format, unsigned int &prevId);

	/// Closes a stream opened with getStreamIf
	/// Releases it ownership
	void			closeStream();

	/// Starts a Button press process.
	/// Waits for any other button process to finish and then start a button process.
	/// There cannot be two button process on the same MFD at the same time.
	/// Can be called from any thread.
	void			startBtnProcess();

	/// Waits until the current button process ends.
	/// Can be called from any thread.
	void			waitForBtnProcess();

	/// Execute the a button process.
	/// Must be called from the main Orbiter thread as it uses Orbiter API.
	void			execBtnProcess(int btnId);

	/// Returns the JSON string with all the button labels
	/// \return All buttons label in a JSON string
	std::string		getJSON();

	/// Returns the JSON string with all the button labels if and only if the prevId is not the current button labels id
	/// The current id can never be 0, so a 0 prevId means that it should always return the JSON string.
	/// \return All buttons label in a JSON string or an empty string
	std::string		getJSONIf(unsigned int &prevId);

	/// Returns true if the close button has been pressed.
	/// Any folower should call this regularly and close the MFD if it returns true.
	/// \return Wether the close button has been pressed or not.
	bool			getClose();

protected:
	/// Virtual destructor.
	/// The MFD must be destroyed by Orbiter when unregistered.
	/// Do NOT delete manualy.
	virtual ~ServerMFD(void);

private:
	/// Called to copy the content of MFD surface to bitmap
	/// This must be called before releasing stream mutex and before calling _generateImage()
	void ServerMFD::_copySurfaceToBitmap();

	/// Called to regenerate the JPEG and PNG images
	/// This must be called while having the ownership of _streamMutex
	void			_generateImage();

	/// Generates the JSON string and stores it in _JSON.
	/// Must be called from the main Orbiter thread.
	void			_generateJSON();

	/// The MFD specifications.
	CppMFDSPEC		_spec;

	/// The mutex to access the streams.
	HANDLE			_streamMutex;

	/// The stream structure for the PNG followers
	imageStream		_streamPNG;

	/// The stream structure for the JPEG followers
	imageStream		_streamJPEG;

	/// The id of the current image. Is incremented at each MFD refresh. Cannot be 0.
	unsigned int	_surfaceId;

	/// The SURFHANDLE used in threads (not managed by the Orbiter core)
	SURFHANDLE		_surface;

	/// Wether the image needs to be regenerated.
	bool			_surfaceHasChanged;

	/// The number of PNG folowers.
	unsigned int	_pngFollowers;

	/// The number of JPEG folowers.
	unsigned int	_jpegFollowers;

	/// The number of folowers with no image interest.
	unsigned int	_noxFollowers;

	/// The mutex to handle the button processes.
	HANDLE			_btnMutex;

	/// The id of the current button labels. Is incremented at each button label change. Cannot be 0.
	unsigned int	_btnLabelsId;

	/// Wether the close button have been pressed or not.
	bool			_btnClose;

	/// The button process binary semaphore: locked when a button process is running.
	/// Is a binary semaphore instead of a mutex because the mutex has thread ownership which breaks the required behaviour.
	HANDLE			_btnProcessSmp;

	/// The labels of all buttons encoded in JSON
	std::string		_JSON;

	/// The copied bitmap from surface
	HBITMAP			_bmpFromSurface;

	/// The mutex to access the image surface
	HANDLE			_imageMutex;
};

#endif // __SERVERMFD_H
