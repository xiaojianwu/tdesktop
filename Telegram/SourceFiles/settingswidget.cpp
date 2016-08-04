/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2016 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"
#include "settingswidget.h"

#include "observer_peer.h"
#include "lang.h"
#include "boxes/aboutbox.h"
#include "mainwidget.h"
#include "application.h"
#include "boxes/photocropbox.h"
#include "boxes/connectionbox.h"
#include "boxes/backgroundbox.h"
#include "boxes/addcontactbox.h"
#include "boxes/emojibox.h"
#include "boxes/confirmbox.h"
#include "boxes/downloadpathbox.h"
#include "boxes/usernamebox.h"
#include "boxes/languagebox.h"
#include "boxes/passcodebox.h"
#include "boxes/autolockbox.h"
#include "boxes/sessionsbox.h"
#include "boxes/stickersetbox.h"
#include "langloaderplain.h"
#include "ui/filedialog.h"
#include "apiwrap.h"
#include "autoupdater.h"
#include "localstorage.h"

Slider::Slider(QWidget *parent, const style::slider &st, int32 count, int32 sel) : QWidget(parent),
_count(count), _sel(snap(sel, 0, _count)), _wasSel(_sel), _st(st), _pressed(false) {
	resize(_st.width, _st.bar.pxHeight());
	setCursor(style::cur_pointer);
}

void Slider::mousePressEvent(QMouseEvent *e) {
	_pressed = true;
	mouseMoveEvent(e);
}

void Slider::mouseMoveEvent(QMouseEvent *e) {
	if (_pressed) {
		int32 newSel = snap(qRound((_count - 1) * float64(e->pos().x() - _st.bar.pxWidth() / 2) / (width() - _st.bar.pxWidth())), 0, _count - 1);
		if (newSel != _sel) {
			_sel = newSel;
			update();
		}
	}
}

void Slider::mouseReleaseEvent(QMouseEvent *e) {
	_pressed = false;
	if (_sel != _wasSel) {
		emit changed(_wasSel);
		_wasSel = _sel;
	}
}

int32 Slider::selected() const {
	return _sel;
}

void Slider::setSelected(int32 sel) {
	if (_sel != sel) {
		_sel = sel;
		emit changed(_wasSel);
		_wasSel = _sel;
		update();
	}
}

void Slider::paintEvent(QPaintEvent *e) {
	Painter p(this);

	p.fillRect(0, (height() - _st.thickness) / 2, width(), _st.thickness, _st.color->b);

	int32 x = qFloor(_sel * float64(width() - _st.bar.pxWidth()) / (_count - 1)), y = (height() - _st.bar.pxHeight()) / 2;
	p.drawSprite(QPoint(x, y), _st.bar);
}

QString scaleLabel(DBIScale scale) {
	switch (scale) {
	case dbisOne: return qsl("100%");
	case dbisOneAndQuarter: return qsl("125%");
	case dbisOneAndHalf: return qsl("150%");
	case dbisTwo: return qsl("200%");
	}
	return QString();
}

bool scaleIs(DBIScale scale) {
	return cRealScale() == scale || (cRealScale() == dbisAuto && cScreenScale() == scale);
}

SettingsInner::SettingsInner(SettingsWidget *parent) : TWidget(parent)
, _self(App::self())

// profile
, _nameCache(self() ? self()->name : QString())
, _uploadPhoto(this, lang(lng_settings_upload), st::btnSetUpload)
, _cancelPhoto(this, lang(lng_cancel))
, _nameOver(false)
, _photoOver(false)
, a_photoOver(0)
, _a_photo(animation(this, &SettingsInner::step_photo))

// contact info
, _phoneText(self() ? App::formatPhone(self()->phone()) : QString())
, _chooseUsername(this, (self() && !self()->username.isEmpty()) ? ('@' + self()->username) : lang(lng_settings_choose_username))

// notifications
, _desktopNotify(this, lang(lng_settings_desktop_notify), cDesktopNotify())
, _senderName(this, lang(lng_settings_show_name), cNotifyView() <= dbinvShowName)
, _messagePreview(this, lang(lng_settings_show_preview), cNotifyView() <= dbinvShowPreview)
, _windowsNotifications(this, lang(lng_settings_use_windows), cWindowsNotifications())
, _soundNotify(this, lang(lng_settings_sound_notify), cSoundNotify())
, _includeMuted(this, lang(lng_settings_include_muted), cIncludeMuted())

// general
, _changeLanguage(this, lang(lng_settings_change_lang))
#ifndef TDESKTOP_DISABLE_AUTOUPDATE
, _autoUpdate(this, lang(lng_settings_auto_update), cAutoUpdate())
, _checkNow(this, lang(lng_settings_check_now))
, _restartNow(this, lang(lng_settings_update_now))
#endif

, _supportTray(cSupportTray())
, _workmodeTray(this, lang(lng_settings_workmode_tray), (cWorkMode() == dbiwmTrayOnly || cWorkMode() == dbiwmWindowAndTray))
, _workmodeWindow(this, lang(lng_settings_workmode_window), (cWorkMode() == dbiwmWindowOnly || cWorkMode() == dbiwmWindowAndTray))

, _autoStart(this, lang(lng_settings_auto_start), cAutoStart())
, _startMinimized(this, lang(lng_settings_start_min), cStartMinimized())
, _sendToMenu(this, lang(lng_settings_add_sendto), cSendToMenu())

, _dpiAutoScale(this, lng_settings_scale_auto(lt_cur, scaleLabel(cScreenScale())), (cConfigScale() == dbisAuto))
, _dpiSlider(this, st::dpiSlider, dbisScaleCount - 1, cEvalScale(cConfigScale()) - 1)
, _dpiWidth1(st::dpiFont1->width(scaleLabel(dbisOne)))
, _dpiWidth2(st::dpiFont2->width(scaleLabel(dbisOneAndQuarter)))
, _dpiWidth3(st::dpiFont3->width(scaleLabel(dbisOneAndHalf)))
, _dpiWidth4(st::dpiFont4->width(scaleLabel(dbisTwo)))

// chat options
, _replaceEmojis(this, lang(lng_settings_replace_emojis), cReplaceEmojis())
, _viewEmojis(this, lang(lng_settings_view_emojis))
, _stickers(this, lang(lng_stickers_you_have))

, _enterSend(this, qsl("send_key"), 0, lang(lng_settings_send_enter), !cCtrlEnter())
, _ctrlEnterSend(this, qsl("send_key"), 1, lang((cPlatform() == dbipMac || cPlatform() == dbipMacOld) ? lng_settings_send_cmdenter : lng_settings_send_ctrlenter), cCtrlEnter())

, _dontAskDownloadPath(this, lang(lng_download_path_dont_ask), !cAskDownloadPath())
, _downloadPathWidth(st::linkFont->width(lang(lng_download_path_label)) + st::linkFont->spacew)
, _downloadPathEdit(this, cDownloadPath().isEmpty() ? lang(lng_download_path_default) : ((cDownloadPath() == qsl("tmp")) ? lang(lng_download_path_temp) : st::linkFont->elided(QDir::toNativeSeparators(cDownloadPath()), st::setWidth - st::cbDefFlat.textLeft - _downloadPathWidth)))
, _downloadPathClear(this, lang(lng_download_path_clear))
, _tempDirClearingWidth(st::linkFont->width(lang(lng_download_path_clearing)))
, _tempDirClearedWidth(st::linkFont->width(lang(lng_download_path_cleared)))
, _tempDirClearFailedWidth(st::linkFont->width(lang(lng_download_path_clear_failed)))

, _autoDownload(this, lang(lng_media_auto_settings))

// local storage
, _localStorageClear(this, lang(lng_local_storage_clear))
, _localStorageHeight(1)
, _storageClearingWidth(st::linkFont->width(lang(lng_local_storage_clearing)))
, _storageClearedWidth(st::linkFont->width(lang(lng_local_storage_cleared)))
, _storageClearFailedWidth(st::linkFont->width(lang(lng_local_storage_clear_failed)))

// chat background
, _backFromGallery(this, lang(lng_settings_bg_from_gallery))
, _backFromFile(this, lang(lng_settings_bg_from_file))
, _tileBackground(this, lang(lng_settings_bg_tile), cTileBackground())
, _adaptiveForWide(this, lang(lng_settings_adaptive_wide), Global::AdaptiveForWide())
, _needBackgroundUpdate(false)
, _radial(animation(this, &SettingsInner::step_radial))

// advanced
, _passcodeEdit(this, lang(cHasPasscode() ? lng_passcode_change : lng_passcode_turn_on))
, _passcodeTurnOff(this, lang(lng_passcode_turn_off))
, _autoLock(this, (cAutoLock() % 3600) ? lng_passcode_autolock_minutes(lt_count, cAutoLock() / 60) : lng_passcode_autolock_hours(lt_count, cAutoLock() / 3600))
, _autoLockText(lang(psIdleSupported() ? lng_passcode_autolock_away : lng_passcode_autolock_inactive) + ' ')
, _autoLockWidth(st::linkFont->width(_autoLockText))
, _passwordEdit(this, lang(lng_cloud_password_set))
, _passwordTurnOff(this, lang(lng_passcode_turn_off))
, _hasPasswordRecovery(false)
, _connectionType(this, lang(lng_connection_auto_connecting))
, _connectionTypeText(lang(lng_connection_type) + ' ')
, _connectionTypeWidth(st::linkFont->width(_connectionTypeText))
, _showSessions(this, lang(lng_settings_show_sessions))
, _askQuestion(this, lang(lng_settings_ask_question))
, _telegramFAQ(this, lang(lng_settings_faq))
, _logOut(this, lang(lng_settings_logout), st::btnRedLink)
, _supportGetRequest(0) {
	Notify::registerPeerObserver(Notify::PeerUpdate::Flag::UsernameChanged, this, &SettingsInner::notifyPeerUpdated);

	App::clearMousedItems();

	if (self()) {
		self()->loadUserpic();

		connect(App::wnd(), SIGNAL(imageLoaded()), this, SLOT(update()));
		connect(App::api(), SIGNAL(fullPeerUpdated(PeerData*)), this, SLOT(onFullPeerUpdated(PeerData*)));

		_nameText.setText(st::setNameFont, _nameCache, _textNameOptions);
		PhotoData *selfPhoto = (self()->photoId && self()->photoId != UnknownPeerPhotoId) ? App::photo(self()->photoId) : 0;
		if (selfPhoto && selfPhoto->date) {
			_photoLink.reset(new PhotoOpenClickHandler(selfPhoto, self()));
		}
		App::api()->requestFullPeer(self());
		onReloadPassword();

		connect(App::main(), SIGNAL(peerPhotoChanged(PeerData *)), this, SLOT(peerUpdated(PeerData *)));
		connect(App::main(), SIGNAL(peerNameChanged(PeerData *, const PeerData::Names &, const PeerData::NameFirstChars &)), this, SLOT(peerUpdated(PeerData *)));

		Sandbox::connect(SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(onReloadPassword(Qt::ApplicationState)));
	}

	// profile
	connect(&_uploadPhoto, SIGNAL(clicked()), this, SLOT(onUpdatePhoto()));
	connect(&_cancelPhoto, SIGNAL(clicked()), this, SLOT(onUpdatePhotoCancel()));

	connect(App::app(), SIGNAL(peerPhotoDone(PeerId)), this, SLOT(onPhotoUpdateDone(PeerId)));
	connect(App::app(), SIGNAL(peerPhotoFail(PeerId)), this, SLOT(onPhotoUpdateFail(PeerId)));

	// contact info
	connect(&_chooseUsername, SIGNAL(clicked()), this, SLOT(onUsername()));

	// notifications
	_senderName.setDisabled(!_desktopNotify.checked());
	_messagePreview.setDisabled(_senderName.disabled() || !_senderName.checked());
	connect(&_desktopNotify, SIGNAL(changed()), this, SLOT(onDesktopNotify()));
	connect(&_senderName, SIGNAL(changed()), this, SLOT(onSenderName()));
	connect(&_messagePreview, SIGNAL(changed()), this, SLOT(onMessagePreview()));
	connect(&_windowsNotifications, SIGNAL(changed()), this, SLOT(onWindowsNotifications()));
	connect(&_soundNotify, SIGNAL(changed()), this, SLOT(onSoundNotify()));
	connect(&_includeMuted, SIGNAL(changed()), this, SLOT(onIncludeMuted()));

	// general
	connect(&_changeLanguage, SIGNAL(clicked()), this, SLOT(onChangeLanguage()));
	#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	connect(&_autoUpdate, SIGNAL(changed()), this, SLOT(onAutoUpdate()));
	connect(&_checkNow, SIGNAL(clicked()), this, SLOT(onCheckNow()));
	connect(&_restartNow, SIGNAL(clicked()), this, SLOT(onRestartNow()));
	#endif

	connect(&_workmodeTray, SIGNAL(changed()), this, SLOT(onWorkmodeTray()));
	connect(&_workmodeWindow, SIGNAL(changed()), this, SLOT(onWorkmodeWindow()));

	_startMinimized.setDisabled(!_autoStart.checked());
	connect(&_autoStart, SIGNAL(changed()), this, SLOT(onAutoStart()));
	connect(&_startMinimized, SIGNAL(changed()), this, SLOT(onStartMinimized()));
	connect(&_sendToMenu, SIGNAL(changed()), this, SLOT(onSendToMenu()));

	connect(&_dpiAutoScale, SIGNAL(changed()), this, SLOT(onScaleAuto()));
	connect(&_dpiSlider, SIGNAL(changed(int32)), this, SLOT(onScaleChange()));

	_curVersionText = lng_settings_current_version(lt_version, QString::fromLatin1(AppVersionStr.c_str()) + (cAlphaVersion() ? " alpha" : "") + (cBetaVersion() ? qsl(" beta %1").arg(cBetaVersion()) : QString())) + ' ';
	_curVersionWidth = st::linkFont->width(_curVersionText);
	_newVersionText = lang(lng_settings_update_ready) + ' ';
	_newVersionWidth = st::linkFont->width(_newVersionText);

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	Sandbox::connect(SIGNAL(updateChecking()), this, SLOT(onUpdateChecking()));
	Sandbox::connect(SIGNAL(updateLatest()), this, SLOT(onUpdateLatest()));
	Sandbox::connect(SIGNAL(updateProgress(qint64,qint64)), this, SLOT(onUpdateDownloading(qint64,qint64)));
	Sandbox::connect(SIGNAL(updateFailed()), this, SLOT(onUpdateFailed()));
	Sandbox::connect(SIGNAL(updateReady()), this, SLOT(onUpdateReady()));
#endif

	// chat options
	connect(&_replaceEmojis, SIGNAL(changed()), this, SLOT(onReplaceEmojis()));
	connect(&_viewEmojis, SIGNAL(clicked()), this, SLOT(onViewEmojis()));
	connect(&_stickers, SIGNAL(clicked()), this, SLOT(onStickers()));

	connect(&_enterSend, SIGNAL(changed()), this, SLOT(onEnterSend()));
	connect(&_ctrlEnterSend, SIGNAL(changed()), this, SLOT(onCtrlEnterSend()));

	connect(&_dontAskDownloadPath, SIGNAL(changed()), this, SLOT(onDontAskDownloadPath()));
	connect(&_downloadPathEdit, SIGNAL(clicked()), this, SLOT(onDownloadPathEdit()));
	connect(&_downloadPathClear, SIGNAL(clicked()), this, SLOT(onDownloadPathClear()));
	switch (App::wnd()->tempDirState()) {
	case MainWindow::TempDirEmpty: _tempDirClearState = TempDirEmpty; break;
	case MainWindow::TempDirExists: _tempDirClearState = TempDirExists; break;
	case MainWindow::TempDirRemoving: _tempDirClearState = TempDirClearing; break;
	}
	connect(App::wnd(), SIGNAL(tempDirCleared(int)), this, SLOT(onTempDirCleared(int)));
	connect(App::wnd(), SIGNAL(tempDirClearFailed(int)), this, SLOT(onTempDirClearFailed(int)));
	connect(&_autoDownload, SIGNAL(clicked()), this, SLOT(onAutoDownload()));

	// local storage
	connect(&_localStorageClear, SIGNAL(clicked()), this, SLOT(onLocalStorageClear()));
	switch (App::wnd()->localStorageState()) {
	case MainWindow::TempDirEmpty: _storageClearState = TempDirEmpty; break;
	case MainWindow::TempDirExists: _storageClearState = TempDirExists; break;
	case MainWindow::TempDirRemoving: _storageClearState = TempDirClearing; break;
	}

	// chat background
	if (!cChatBackground()) App::initBackground();
	updateChatBackground();
	connect(&_backFromGallery, SIGNAL(clicked()), this, SLOT(onBackFromGallery()));
	connect(&_backFromFile, SIGNAL(clicked()), this, SLOT(onBackFromFile()));
	connect(&_tileBackground, SIGNAL(changed()), this, SLOT(onTileBackground()));
	connect(&_adaptiveForWide, SIGNAL(changed()), this, SLOT(onAdaptiveForWide()));

	if (radialLoading()) {
		radialStart();
	}

	// advanced
	connect(&_passcodeEdit, SIGNAL(clicked()), this, SLOT(onPasscode()));
	connect(&_passcodeTurnOff, SIGNAL(clicked()), this, SLOT(onPasscodeOff()));
	connect(&_autoLock, SIGNAL(clicked()), this, SLOT(onAutoLock()));
	connect(&_passwordEdit, SIGNAL(clicked()), this, SLOT(onPassword()));
	connect(&_passwordTurnOff, SIGNAL(clicked()), this, SLOT(onPasswordOff()));
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
	connect(&_connectionType, SIGNAL(clicked()), this, SLOT(onConnectionType()));
#endif
	connect(&_showSessions, SIGNAL(clicked()), this, SLOT(onShowSessions()));
	connect(&_askQuestion, SIGNAL(clicked()), this, SLOT(onAskQuestion()));
	connect(&_telegramFAQ, SIGNAL(clicked()), this, SLOT(onTelegramFAQ()));
	connect(&_logOut, SIGNAL(clicked()), App::wnd(), SLOT(onLogout()));

    if (App::main()) {
        connect(App::main(), SIGNAL(peerUpdated(PeerData*)), this, SLOT(peerUpdated(PeerData*)));
    }

	updateOnlineDisplay();

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	switch (Sandbox::updatingState()) {
	case Application::UpdatingDownload:
		setUpdatingState(UpdatingDownload, true);
		setDownloadProgress(Sandbox::updatingReady(), Sandbox::updatingSize());
	break;
	case Application::UpdatingReady: setUpdatingState(UpdatingReady, true); break;
	default: setUpdatingState(UpdatingNone, true); break;
	}
#endif

	updateConnectionType();

	setMouseTracking(true);
}

void SettingsInner::notifyPeerUpdated(const Notify::PeerUpdate &update) {
	if (update.peer == App::self()) {
		if (update.flags & Notify::PeerUpdate::Flag::UsernameChanged) {
			usernameChanged();
		}
	}
}

void SettingsInner::peerUpdated(PeerData *data) {
	if (self() && data == self()) {
		if (self()->photoId && self()->photoId != UnknownPeerPhotoId) {
			PhotoData *selfPhoto = App::photo(self()->photoId);
			if (selfPhoto->date) {
				_photoLink.reset(new PhotoOpenClickHandler(selfPhoto, self()));
			} else {
				_photoLink.clear();
				App::api()->requestFullPeer(self());
			}
		} else {
			_photoLink.clear();
		}

		if (_nameCache != self()->name) {
			_nameCache = self()->name;
			_nameText.setText(st::setNameFont, _nameCache, _textNameOptions);
			update();
		}
	}
}

void SettingsInner::paintEvent(QPaintEvent *e) {
	Painter p(this);

	p.setClipRect(e->rect());

	int32 top = 0;
	if (self()) {
		// profile
		top += st::setTop;

		_nameText.drawElided(p, _uploadPhoto.x() + st::setNameLeft, top + st::setNameTop, _uploadPhoto.width() - st::setNameLeft);
		if (!_cancelPhoto.isHidden()) {
			p.setFont(st::linkFont->f);
			p.setPen(st::black->p);
			p.drawText(_uploadPhoto.x() + st::setStatusLeft, _cancelPhoto.y() + st::linkFont->ascent, lang(lng_settings_uploading_photo));
		}

		if (_photoLink) {
			self()->paintUserpicLeft(p, st::setPhotoSize, _left, top, st::setWidth);
		} else {
			if (a_photoOver.current() < 1) {
				p.drawSprite(QPoint(_left, top), st::setPhotoImg);
			}
			if (a_photoOver.current() > 0) {
				p.setOpacity(a_photoOver.current());
				p.drawSprite(QPoint(_left, top), st::setOverPhotoImg);
				p.setOpacity(1);
			}
		}

		p.setFont(st::setStatusFont->f);
		bool connecting = App::wnd()->connectingVisible();
		p.setPen((connecting ? st::profileOfflineFg : st::profileOnlineFg)->p);
		p.drawText(_uploadPhoto.x() + st::setStatusLeft, top + st::setStatusTop + st::setStatusFont->ascent, lang(connecting ? lng_status_connecting : lng_status_online));

		top += st::setPhotoSize;

		if (!_errorText.isEmpty()) {
			p.setFont(st::setErrFont->f);
			p.setPen(st::setErrColor->p);
			p.drawText(QRect(_uploadPhoto.x(), _uploadPhoto.y() + _uploadPhoto.height() + st::setLittleSkip, _uploadPhoto.width(), st::setErrFont->height), _errorText, style::al_center);
		}

		// contact info
		p.setFont(st::setHeaderFont->f);
		p.setPen(st::setHeaderColor->p);
		p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_contact_info));
		top += st::setHeaderSkip;

		p.setFont(st::linkFont->f);
		p.setPen(st::black->p);
		p.drawText(_left, top + st::linkFont->ascent, lang(lng_settings_phone_number));
		p.drawText(_left + st::setContactInfoLeft, top + st::linkFont->ascent, _phoneText);
		top += st::linkFont->height + st::setLittleSkip;

		p.drawText(_left, top + st::linkFont->ascent, lang(lng_settings_username));
		top += st::linkFont->height;

		// notifications
		p.setFont(st::setHeaderFont->f);
		p.setPen(st::setHeaderColor->p);
		p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_notify));
		top += st::setHeaderSkip;

		top += _desktopNotify.height() + st::setLittleSkip;
		top += _senderName.height() + st::setLittleSkip;
		top += _messagePreview.height() + st::setSectionSkip;
		if (App::wnd()->psHasNativeNotifications() && cPlatform() == dbipWindows) {
			top += _windowsNotifications.height() + st::setSectionSkip;
		}
		top += _soundNotify.height() + st::setSectionSkip;
		top += _includeMuted.height();
	}

	// general
	p.setFont(st::setHeaderFont->f);
	p.setPen(st::setHeaderColor->p);
	p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_general));
	top += st::setHeaderSkip;

	#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	top += _autoUpdate.height();
	QString textToDraw;
	if (cAutoUpdate()) {
		switch (_updatingState) {
		case UpdatingNone: textToDraw = _curVersionText; break;
		case UpdatingCheck: textToDraw = lang(lng_settings_update_checking); break;
		case UpdatingLatest: textToDraw = lang(lng_settings_latest_installed); break;
		case UpdatingDownload: textToDraw = _newVersionDownload; break;
		case UpdatingReady: textToDraw = _newVersionText; break;
		case UpdatingFail: textToDraw = lang(lng_settings_update_fail); break;
		}
	} else {
		textToDraw = _curVersionText;
	}
	p.setFont(st::linkFont);
	p.setPen(st::setVersionColor);
	p.drawText(_left + st::setVersionLeft, top + st::setVersionTop + st::linkFont->ascent, textToDraw);
	top += st::setVersionHeight;
	#endif

    if (cPlatform() == dbipWindows) {
        top += _workmodeTray.height() + st::setLittleSkip;
        top += _workmodeWindow.height() + st::setSectionSkip;

        top += _autoStart.height() + st::setLittleSkip;
        top += _startMinimized.height() + st::setSectionSkip;

		top += _sendToMenu.height();
    } else if (_supportTray) {
		top += _workmodeTray.height();
	}

    if (!cRetina()) {
        p.setFont(st::setHeaderFont->f);
        p.setPen(st::setHeaderColor->p);
        p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_scale_label));
        top += st::setHeaderSkip;
        top += _dpiAutoScale.height() + st::setLittleSkip;

        top += _dpiSlider.height() + st::dpiFont4->height;
        int32 sLeft = _dpiSlider.x() + _dpiWidth1 / 2, sWidth = _dpiSlider.width();
        float64 sStep = (sWidth - _dpiWidth1 / 2 - _dpiWidth4 / 2) / float64(dbisScaleCount - 2);
        p.setFont(st::dpiFont1->f);

        p.setPen((scaleIs(dbisOne) ? st::dpiActive : st::dpiInactive)->p);
        p.drawText(sLeft + qRound(0 * sStep) - _dpiWidth1 / 2, top - (st::dpiFont4->height - st::dpiFont1->height) / 2 - st::dpiFont1->descent, scaleLabel(dbisOne));
        p.setFont(st::dpiFont2->f);
        p.setPen((scaleIs(dbisOneAndQuarter) ? st::dpiActive : st::dpiInactive)->p);
        p.drawText(sLeft + qRound(1 * sStep) - _dpiWidth2 / 2, top - (st::dpiFont4->height - st::dpiFont2->height) / 2 - st::dpiFont2->descent, scaleLabel(dbisOneAndQuarter));
        p.setFont(st::dpiFont3->f);
        p.setPen((scaleIs(dbisOneAndHalf) ? st::dpiActive : st::dpiInactive)->p);
        p.drawText(sLeft + qRound(2 * sStep) - _dpiWidth3 / 2, top - (st::dpiFont4->height - st::dpiFont3->height) / 2 - st::dpiFont3->descent, scaleLabel(dbisOneAndHalf));
        p.setFont(st::dpiFont4->f);
        p.setPen((scaleIs(dbisTwo) ? st::dpiActive : st::dpiInactive)->p);
        p.drawText(sLeft + qRound(3 * sStep) - _dpiWidth4 / 2, top - (st::dpiFont4->height - st::dpiFont4->height) / 2 - st::dpiFont4->descent, scaleLabel(dbisTwo));
        p.setFont(st::linkFont->f);
    }

	if (self()) {
		// chat options
		p.setFont(st::setHeaderFont->f);
		p.setPen(st::setHeaderColor->p);
		p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_chat));
		top += st::setHeaderSkip;

		top += _replaceEmojis.height() + st::setLittleSkip;
		top += _stickers.height() + st::setSectionSkip;
		top += _enterSend.height() + st::setLittleSkip;
		top += _ctrlEnterSend.height() + st::setSectionSkip;

		top += _dontAskDownloadPath.height();
		if (!cAskDownloadPath()) {
			top += st::setLittleSkip;
			p.setFont(st::linkFont->f);
			p.setPen(st::black->p);
			p.drawText(_left + st::cbDefFlat.textLeft, top + st::linkFont->ascent, lang(lng_download_path_label));
			if (cDownloadPath() == qsl("tmp")) {
				QString clearText;
				int32 clearWidth = 0;
				switch (_tempDirClearState) {
				case TempDirClearing: clearText = lang(lng_download_path_clearing); clearWidth = _tempDirClearingWidth; break;
				case TempDirCleared: clearText = lang(lng_download_path_cleared); clearWidth = _tempDirClearedWidth; break;
				case TempDirClearFailed: clearText = lang(lng_download_path_clear_failed); clearWidth = _tempDirClearFailedWidth; break;
				}
				if (clearWidth) {
					p.drawText(_left + st::setWidth - clearWidth, top + st::linkFont->ascent, clearText);
				}
			}
			top += _downloadPathEdit.height();
		}
		top += st::setLittleSkip;
		top += _autoDownload.height();

		// local storage
		p.setFont(st::setHeaderFont->f);
		p.setPen(st::setHeaderColor->p);
		p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_cache));

		p.setFont(st::linkFont->f);
		p.setPen(st::black->p);
		QString clearText;
		int32 clearWidth = 0;
		switch (_storageClearState) {
		case TempDirClearing: clearText = lang(lng_local_storage_clearing); clearWidth = _storageClearingWidth; break;
		case TempDirCleared: clearText = lang(lng_local_storage_cleared); clearWidth = _storageClearedWidth; break;
		case TempDirClearFailed: clearText = lang(lng_local_storage_clear_failed); clearWidth = _storageClearFailedWidth; break;
		}
		if (clearWidth) {
			p.drawText(_left + st::setWidth - clearWidth, top + st::setHeaderTop + st::setHeaderFont->ascent, clearText);
		}

		top += st::setHeaderSkip;

		int32 cntImages = Local::hasImages() + Local::hasStickers() + Local::hasWebFiles(), cntAudios = Local::hasAudios();
		if (cntImages > 0 && cntAudios > 0) {
			if (_localStorageHeight != 2) {
				cntAudios = 0;
				QTimer::singleShot(0, this, SLOT(onUpdateLocalStorage()));
			}
		} else {
			if (_localStorageHeight != 1) {
				QTimer::singleShot(0, this, SLOT(onUpdateLocalStorage()));
			}
		}
		if (cntImages > 0) {
			QString cnt = lng_settings_images_cached(lt_count, cntImages, lt_size, formatSizeText(Local::storageImagesSize() + Local::storageStickersSize() + Local::storageWebFilesSize()));
			p.drawText(_left + st::setHeaderLeft, top + st::linkFont->ascent, cnt);
		}
		if (_localStorageHeight == 2) top += _localStorageClear.height() + st::setLittleSkip;
		if (cntAudios > 0) {
			QString cnt = lng_settings_audios_cached(lt_count, cntAudios, lt_size, formatSizeText(Local::storageAudiosSize()));
			p.drawText(_left + st::setHeaderLeft, top + st::linkFont->ascent, cnt);
		} else if (cntImages <= 0) {
			p.drawText(_left + st::setHeaderLeft, top + st::linkFont->ascent, lang(lng_settings_no_data_cached));
		}
		top += _localStorageClear.height();

		// chat background
		p.setFont(st::setHeaderFont->f);
		p.setPen(st::setHeaderColor->p);
		p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_background));
		top += st::setHeaderSkip;

		bool radial = false;
		float64 radialOpacity = 0;
		if (_radial.animating()) {
			_radial.step(getms());
			radial = _radial.animating();
			radialOpacity = _radial.opacity();
		}
		if (radial) {
			auto backThumb = App::main() ? App::main()->newBackgroundThumb() : ImagePtr();
			if (backThumb->isNull()) {
				p.drawPixmap(_left, top, _background);
			} else {
				const QPixmap &pix = App::main()->newBackgroundThumb()->pixBlurred(st::setBackgroundSize);
				p.drawPixmap(_left, top, st::setBackgroundSize, st::setBackgroundSize, pix, 0, (pix.height() - st::setBackgroundSize) / 2, st::setBackgroundSize, st::setBackgroundSize);
			}

			auto outer = radialRect();
			QRect inner(QPoint(outer.x() + (outer.width() - st::radialSize.width()) / 2, outer.y() + (outer.height() - st::radialSize.height()) / 2), st::radialSize);
			p.setPen(Qt::NoPen);
			p.setBrush(st::black);
			p.setOpacity(radialOpacity * st::radialBgOpacity);

			p.setRenderHint(QPainter::HighQualityAntialiasing);
			p.drawEllipse(inner);
			p.setRenderHint(QPainter::HighQualityAntialiasing, false);

			p.setOpacity(1);
			QRect arc(inner.marginsRemoved(QMargins(st::radialLine, st::radialLine, st::radialLine, st::radialLine)));
			_radial.draw(p, arc, st::radialLine, st::white);
		} else {
			p.drawPixmap(_left, top, _background);
		}
		top += st::setBackgroundSize;
		top += st::setLittleSkip;
		top += _tileBackground.height();
		if (Global::AdaptiveLayout() == Adaptive::WideLayout) {
			top += st::setLittleSkip;
			top += _adaptiveForWide.height();
		}
	}

	// advanced
	p.setFont(st::setHeaderFont->f);
	p.setPen(st::setHeaderColor->p);
	p.drawText(_left + st::setHeaderLeft, top + st::setHeaderTop + st::setHeaderFont->ascent, lang(lng_settings_section_advanced));
	top += st::setHeaderSkip;

	p.setFont(st::linkFont->f);
	p.setPen(st::black->p);
	if (self()) {
		top += _passcodeEdit.height() + st::setLittleSkip;
		if (cHasPasscode()) {
			p.drawText(_left, top + st::linkFont->ascent, _autoLockText);
			top += _autoLock.height() + st::setLittleSkip;
		}
		if (!_waitingConfirm.isEmpty()) p.drawText(_left, top + st::linkFont->ascent, _waitingConfirm);
		top += _passwordEdit.height() + st::setLittleSkip;
	}

	p.drawText(_left, _connectionType.y() + st::linkFont->ascent, _connectionTypeText);
}

void SettingsInner::resizeEvent(QResizeEvent *e) {
	_left = (width() - st::setWidth) / 2;

	int32 top = 0;

	if (self()) {
		// profile
		top += st::setTop;
		top += st::setPhotoSize;
		_uploadPhoto.move(_left + st::setWidth - _uploadPhoto.width(), top - _uploadPhoto.height());
		_cancelPhoto.move(_left + st::setWidth - _cancelPhoto.width(), top - _uploadPhoto.height() + st::btnSetUpload.textTop + st::btnSetUpload.font->ascent - st::linkFont->ascent);

		// contact info
		top += st::setHeaderSkip;
		top += st::linkFont->height + st::setLittleSkip;
		_chooseUsername.move(_left + st::setContactInfoLeft, top); top += st::linkFont->height;

		// notifications
		top += st::setHeaderSkip;
		_desktopNotify.move(_left, top); top += _desktopNotify.height() + st::setLittleSkip;
		_senderName.move(_left, top); top += _senderName.height() + st::setLittleSkip;
		_messagePreview.move(_left, top); top += _messagePreview.height() + st::setSectionSkip;
		if (App::wnd()->psHasNativeNotifications() && cPlatform() == dbipWindows) {
			_windowsNotifications.move(_left, top); top += _windowsNotifications.height() + st::setSectionSkip;
		}
		_soundNotify.move(_left, top); top += _soundNotify.height() + st::setSectionSkip;
		_includeMuted.move(_left, top); top += _includeMuted.height();
	}

	// general
	top += st::setHeaderSkip;
	_changeLanguage.move(_left + st::setWidth - _changeLanguage.width(), top - st::setHeaderSkip + st::setHeaderTop + st::setHeaderFont->ascent - st::linkFont->ascent);
	#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	_autoUpdate.move(_left, top);
	_checkNow.move(_left + st::setWidth - _checkNow.width(), top + st::cbDefFlat.textTop); top += _autoUpdate.height();
	_restartNow.move(_left + st::setWidth - _restartNow.width(), top + st::setVersionTop);
	top += st::setVersionHeight;
	#endif

    if (cPlatform() == dbipWindows) {
        _workmodeTray.move(_left, top); top += _workmodeTray.height() + st::setLittleSkip;
        _workmodeWindow.move(_left, top); top += _workmodeWindow.height() + st::setSectionSkip;

        _autoStart.move(_left, top); top += _autoStart.height() + st::setLittleSkip;
        _startMinimized.move(_left, top); top += _startMinimized.height() + st::setSectionSkip;

		_sendToMenu.move(_left, top); top += _sendToMenu.height();
    } else if (_supportTray) {
		_workmodeTray.move(_left, top); top += _workmodeTray.height();
	}
    if (!cRetina()) {
        top += st::setHeaderSkip;
        _dpiAutoScale.move(_left, top); top += _dpiAutoScale.height() + st::setLittleSkip;
        _dpiSlider.move(_left, top); top += _dpiSlider.height() + st::dpiFont4->height;
    }

	// chat options
	if (self()) {
		top += st::setHeaderSkip;
		_viewEmojis.move(_left + st::setWidth - _viewEmojis.width(), top + st::cbDefFlat.textTop);
		_replaceEmojis.move(_left, top); top += _replaceEmojis.height() + st::setLittleSkip;
		_stickers.move(_left + st::cbDefFlat.textLeft, top); top += _stickers.height() + st::setSectionSkip;
		_enterSend.move(_left, top); top += _enterSend.height() + st::setLittleSkip;
		_ctrlEnterSend.move(_left, top); top += _ctrlEnterSend.height() + st::setSectionSkip;
		_dontAskDownloadPath.move(_left, top); top += _dontAskDownloadPath.height();
		if (!cAskDownloadPath()) {
			top += st::setLittleSkip;
			_downloadPathEdit.move(_left + st::cbDefFlat.textLeft + _downloadPathWidth, top);
			if (cDownloadPath() == qsl("tmp")) {
				_downloadPathClear.move(_left + st::setWidth - _downloadPathClear.width(), top);
			}
			top += _downloadPathEdit.height();
		}
		top += st::setLittleSkip;
		_autoDownload.move(_left + st::cbDefFlat.textLeft, top); top += _autoDownload.height();

		// local storage
		_localStorageClear.move(_left + st::setWidth - _localStorageClear.width(), top + st::setHeaderTop + st::setHeaderFont->ascent - st::linkFont->ascent);
		top += st::setHeaderSkip;
		if ((Local::hasImages() || Local::hasStickers() || Local::hasWebFiles()) && Local::hasAudios()) {
			_localStorageHeight = 2;
			top += _localStorageClear.height() + st::setLittleSkip;
		} else {
			_localStorageHeight = 1;
		}
		top += _localStorageClear.height();

		// chat background
		top += st::setHeaderSkip;
		_backFromGallery.move(_left + st::setBackgroundSize + st::setLittleSkip, top);
		_backFromFile.move(_left + st::setBackgroundSize + st::setLittleSkip, top + _backFromGallery.height() + st::setLittleSkip);
		top += st::setBackgroundSize;

		top += st::setLittleSkip;
		_tileBackground.move(_left, top); top += _tileBackground.height();
		if (Global::AdaptiveLayout() == Adaptive::WideLayout) {
			top += st::setLittleSkip;
			_adaptiveForWide.move(_left, top); top += _adaptiveForWide.height();
		}
	}

	// advanced
	top += st::setHeaderSkip;
	if (self()) {
		_passcodeEdit.move(_left, top);
		_passcodeTurnOff.move(_left + st::setWidth - _passcodeTurnOff.width(), top); top += _passcodeTurnOff.height() + st::setLittleSkip;
		if (cHasPasscode()) {
			_autoLock.move(_left + _autoLockWidth, top); top += _autoLock.height() + st::setLittleSkip;
		}
		_passwordEdit.move(_left, top);
		_passwordTurnOff.move(_left + st::setWidth - _passwordTurnOff.width(), top); top += _passwordTurnOff.height() + st::setLittleSkip;
	}

	_connectionType.move(_left + _connectionTypeWidth, top); top += _connectionType.height() + st::setLittleSkip;
	if (self()) {
		_showSessions.move(_left, top); top += _showSessions.height() + st::setSectionSkip;
		_askQuestion.move(_left, top); top += _askQuestion.height() + st::setLittleSkip;
		_telegramFAQ.move(_left, top); top += _telegramFAQ.height() + st::setSectionSkip;
		_logOut.move(_left, top);
	} else {
		top += st::setSectionSkip - st::setLittleSkip;
		_telegramFAQ.move(_left, top);
	}
}

void SettingsInner::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Back) {
		App::wnd()->showSettings();
	}
	_secretText += e->text().toLower();
	int32 size = _secretText.size(), from = 0;
	while (size > from) {
		QStringRef str(_secretText.midRef(from));
		if (str == qstr("debugmode")) {
			QString text = cDebug() ? qsl("Do you want to disable DEBUG logs?") : qsl("Do you want to enable DEBUG logs?\n\nAll network events will be logged.");
			ConfirmBox *box = new ConfirmBox(text);
			connect(box, SIGNAL(confirmed()), App::app(), SLOT(onSwitchDebugMode()));
			Ui::showLayer(box);
			from = size;
			break;
		} else if (str == qstr("testmode")) {
			QString text = cTestMode() ? qsl("Do you want to disable TEST mode?") : qsl("Do you want to enable TEST mode?\n\nYou will be switched to test cloud.");
			ConfirmBox *box = new ConfirmBox(text);
			connect(box, SIGNAL(confirmed()), App::app(), SLOT(onSwitchTestMode()));
			Ui::showLayer(box);
			from = size;
			break;
		} else if (str == qstr("loadlang")) {
			chooseCustomLang();
			break;
		} else if (str == qstr("debugfiles") && cDebug()) {
			if (DebugLogging::FileLoader()) {
				Global::RefDebugLoggingFlags() &= ~DebugLogging::FileLoaderFlag;
			} else {
				Global::RefDebugLoggingFlags() |= DebugLogging::FileLoaderFlag;
			}
			Ui::showLayer(new InformBox(DebugLogging::FileLoader() ? qsl("Enabled file download logging") : qsl("Disabled file download logging")));
			break;
		} else if (str == qstr("crashplease")) {
			t_assert(!"Crashed in Settings!");
			break;
		} else if (str == qstr("workmode")) {
			auto text = Global::DialogsModeEnabled() ? qsl("Disable work mode?") : qsl("Enable work mode?");
			auto box = std_::make_unique<ConfirmBox>(text);
			connect(box.get(), SIGNAL(confirmed()), App::app(), SLOT(onSwitchWorkMode()));
			Ui::showLayer(box.release());
			from = size;
			break;
		} else if (str == qstr("moderate")) {
			auto text = Global::ModerateModeEnabled() ? qsl("Disable moderate mode?") : qsl("Enable moderate mode?");
			auto box = std_::make_unique<ConfirmBox>(text);
			connect(box.get(), SIGNAL(confirmed()), this, SLOT(onSwitchModerateMode()));
			Ui::showLayer(box.release());
			break;
		} else if (str == qstr("clearstickers")) {
			auto box = std_::make_unique<ConfirmBox>(qsl("Clear frequently used stickers list?"));
			connect(box.get(), SIGNAL(confirmed()), this, SLOT(onClearStickers()));
			Ui::showLayer(box.release());
			break;
		} else if (
			qsl("debugmode").startsWith(str) ||
			qsl("testmode").startsWith(str) ||
			qsl("loadlang").startsWith(str) ||
			qsl("clearstickers").startsWith(str) ||
			qsl("moderate").startsWith(str) ||
			qsl("debugfiles").startsWith(str) ||
			qsl("workmode").startsWith(str) ||
			qsl("crashplease").startsWith(str)) {
			break;
		}
		++from;
	}
	_secretText = (size > from) ? _secretText.mid(from) : QString();
}

void SettingsInner::mouseMoveEvent(QMouseEvent *e) {
	if (!self()) {
		setCursor(style::cur_default);
	} else {
		bool nameOver = QRect(_uploadPhoto.x() + st::setNameLeft, st::setTop + st::setNameTop, qMin(_uploadPhoto.width() - int(st::setNameLeft), _nameText.maxWidth()), st::setNameFont->height).contains(e->pos());
		if (nameOver != _nameOver) {
			_nameOver = nameOver;
		}

		bool photoOver = QRect(_left, st::setTop, st::setPhotoSize, st::setPhotoSize).contains(e->pos());
		if (photoOver != _photoOver) {
			_photoOver = photoOver;
			if (!_photoLink) {
				a_photoOver.start(_photoOver ? 1 : 0);
				_a_photo.start();
			}
		}

		setCursor((_nameOver || _photoOver) ? style::cur_pointer : style::cur_default);
	}
}

void SettingsInner::mousePressEvent(QMouseEvent *e) {
	mouseMoveEvent(e);
	if (!self()) {
		return;
	}
	if (QRect(_uploadPhoto.x() + st::setNameLeft, st::setTop + st::setNameTop, qMin(_uploadPhoto.width() - int(st::setNameLeft), _nameText.maxWidth()), st::setNameFont->height).contains(e->pos())) {
		Ui::showLayer(new EditNameTitleBox(self()));
	} else if (QRect(_left, st::setTop, st::setPhotoSize, st::setPhotoSize).contains(e->pos())) {
		if (_photoLink) {
			App::photo(self()->photoId)->download();
			_photoLink->onClick(e->button());
		} else {
			onUpdatePhoto();
		}
	}
}

void SettingsInner::contextMenuEvent(QContextMenuEvent *e) {
}

void SettingsInner::updateAdaptiveLayout() {
	showAll();
	resizeEvent(0);
}

void SettingsInner::step_photo(float64 ms, bool timer) {
	float64 dt = ms / st::setPhotoDuration;
	if (dt >= 1) {
		_a_photo.stop();
		a_photoOver.finish();
	} else {
		a_photoOver.update(dt, anim::linear);
	}
	if (timer) update(_left, st::setTop, st::setPhotoSize, st::setPhotoSize);
}

void SettingsInner::updateSize(int32 newWidth) {
	if (_logOut.isHidden()) {
		resize(newWidth, _telegramFAQ.geometry().bottom() + st::setBottom);
	} else {
		resize(newWidth, _logOut.geometry().bottom() + st::setBottom);
	}
}


void SettingsInner::updateOnlineDisplay() {
}

void SettingsInner::updateConnectionType() {
	QString connection;
	switch (cConnectionType()) {
	case dbictAuto: {
		QString transport = MTP::dctransport();
		connection = transport.isEmpty() ? lang(lng_connection_auto_connecting) : lng_connection_auto(lt_transport, transport);
	} break;
	case dbictHttpProxy:
	case dbictTcpProxy: {
		QString transport = MTP::dctransport();
		connection = transport.isEmpty() ? lang(lng_connection_proxy_connecting) : lng_connection_proxy(lt_transport, transport);
	} break;
	}
	_connectionType.setText(connection);
}

void SettingsInner::passcodeChanged() {
	resizeEvent(0);
	_passcodeEdit.setText(lang(cHasPasscode() ? lng_passcode_change : lng_passcode_turn_on));
	_autoLock.setText((cAutoLock() % 3600) ? lng_passcode_autolock_minutes(lt_count, cAutoLock() / 60) : lng_passcode_autolock_hours(lt_count, cAutoLock() / 3600));
//	_passwordEdit.setText()
	showAll();
}

void SettingsInner::updateBackgroundRect() {
	update(radialRect());
}

void SettingsInner::onFullPeerUpdated(PeerData *peer) {
	if (!self() || self() != peer) return;

	PhotoData *selfPhoto = (self()->photoId && self()->photoId != UnknownPeerPhotoId) ? App::photo(self()->photoId) : 0;
	if (selfPhoto && selfPhoto->date) {
		_photoLink.reset(new PhotoOpenClickHandler(selfPhoto, self()));
	} else {
		_photoLink.clear();
	}
}

void SettingsInner::gotPassword(const MTPaccount_Password &result) {
	_waitingConfirm = QString();

	switch (result.type()) {
	case mtpc_account_noPassword: {
		const auto &d(result.c_account_noPassword());
		_curPasswordSalt = QByteArray();
		_hasPasswordRecovery = false;
		_curPasswordHint = QString();
		_newPasswordSalt = qba(d.vnew_salt);
		QString pattern = qs(d.vemail_unconfirmed_pattern);
		if (!pattern.isEmpty()) _waitingConfirm = lng_cloud_password_waiting(lt_email, pattern);
	} break;

	case mtpc_account_password: {
		const auto &d(result.c_account_password());
		_curPasswordSalt = qba(d.vcurrent_salt);
		_hasPasswordRecovery = mtpIsTrue(d.vhas_recovery);
		_curPasswordHint = qs(d.vhint);
		_newPasswordSalt = qba(d.vnew_salt);
		QString pattern = qs(d.vemail_unconfirmed_pattern);
		if (!pattern.isEmpty()) _waitingConfirm = lng_cloud_password_waiting(lt_email, pattern);
	} break;
	}
	_waitingConfirm = st::linkFont->elided(_waitingConfirm, st::setWidth - _passwordTurnOff.width());
	_passwordEdit.setText(lang(_curPasswordSalt.isEmpty() ? lng_cloud_password_set : lng_cloud_password_edit));
	showAll();
	update();

	_newPasswordSalt.resize(_newPasswordSalt.size() + 8);
	memset_rand(_newPasswordSalt.data() + _newPasswordSalt.size() - 8, 8);
}

void SettingsInner::offPasswordDone(const MTPBool &result) {
	onReloadPassword();
}

bool SettingsInner::offPasswordFail(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	onReloadPassword();
	return true;
}

void SettingsInner::usernameChanged() {
	_chooseUsername.setText((self() && !self()->username.isEmpty()) ? ('@' + self()->username) : lang(lng_settings_choose_username));
	showAll();
	update();
}

void SettingsInner::showAll() {
	// profile
	if (self()) {
		if (App::app()->isPhotoUpdating(self()->id)) {
			_cancelPhoto.show();
			_uploadPhoto.hide();
		} else {
			_cancelPhoto.hide();
			_uploadPhoto.show();
		}
	} else {
		_uploadPhoto.hide();
		_cancelPhoto.hide();
	}

	// contact info
	if (self()) {
		_chooseUsername.show();
	} else {
		_chooseUsername.hide();
	}

	// notifications
	if (self()) {
		_desktopNotify.show();
		_senderName.show();
		_messagePreview.show();
		if (App::wnd()->psHasNativeNotifications() && cPlatform() == dbipWindows) {
			_windowsNotifications.show();
		} else {
			_windowsNotifications.hide();
		}
		_soundNotify.show();
		_includeMuted.show();
	} else {
		_desktopNotify.hide();
		_senderName.hide();
		_messagePreview.hide();
		_windowsNotifications.hide();
		_soundNotify.hide();
		_includeMuted.hide();
	}

	// general
	_changeLanguage.show();
	#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	_autoUpdate.show();
	setUpdatingState(_updatingState, true);
	#endif
    if (cPlatform() == dbipWindows) {
        _workmodeTray.show();
        _workmodeWindow.show();

        _autoStart.show();
        _startMinimized.show();

		_sendToMenu.show();
    } else {
        if (_supportTray) {
			_workmodeTray.show();
		} else {
			_workmodeTray.hide();
		}
        _workmodeWindow.hide();

        _autoStart.hide();
        _startMinimized.hide();

		_sendToMenu.hide();
	}
    if (cRetina()) {
        _dpiSlider.hide();
        _dpiAutoScale.hide();
    } else {
        _dpiSlider.show();
        _dpiAutoScale.show();
    }

	// chat options
	if (self()) {
		_replaceEmojis.show();
		if (cReplaceEmojis()) {
			_viewEmojis.show();
		} else {
			_viewEmojis.hide();
		}
		_stickers.show();
		_enterSend.show();
		_ctrlEnterSend.show();
		_dontAskDownloadPath.show();
		if (cAskDownloadPath()) {
			_downloadPathEdit.hide();
			_downloadPathClear.hide();
		} else {
			_downloadPathEdit.show();
			if (cDownloadPath() == qsl("tmp") && _tempDirClearState == TempDirExists) { // dir exists, not clearing right now
				_downloadPathClear.show();
			} else {
				_downloadPathClear.hide();
			}
		}
		_autoDownload.show();
	} else {
		_replaceEmojis.hide();
		_viewEmojis.hide();
		_stickers.hide();
		_enterSend.hide();
		_ctrlEnterSend.hide();
		_dontAskDownloadPath.hide();
		_downloadPathEdit.hide();
		_downloadPathClear.hide();
		_autoDownload.hide();
	}

	// local storage
	if (self() && _storageClearState == TempDirExists) {
		_localStorageClear.show();
	} else {
		_localStorageClear.hide();
	}

	// chat background
	if (self()) {
		_backFromGallery.show();
		_backFromFile.show();
		_tileBackground.show();
		if (Global::AdaptiveLayout() == Adaptive::WideLayout) {
			_adaptiveForWide.show();
		} else {
			_adaptiveForWide.hide();
		}
	} else {
		_backFromGallery.hide();
		_backFromFile.hide();
		_tileBackground.hide();
		_adaptiveForWide.hide();
	}

	// advanced
	if (self()) {
		_passcodeEdit.show();
		if (cHasPasscode()) {
			_autoLock.show();
			_passcodeTurnOff.show();
		} else {
			_autoLock.hide();
			_passcodeTurnOff.hide();
		}
		if (_waitingConfirm.isEmpty()) {
			_passwordEdit.show();
		} else {
			_passwordEdit.hide();
		}
		if (_curPasswordSalt.isEmpty() && _waitingConfirm.isEmpty()) {
			_passwordTurnOff.hide();
		} else {
			_passwordTurnOff.show();
		}
		_showSessions.show();
		_askQuestion.show();
		_logOut.show();
	} else {
		_passcodeEdit.hide();
		_autoLock.hide();
		_passcodeTurnOff.hide();
		_passwordEdit.hide();
		_passwordTurnOff.hide();
		_showSessions.hide();
		_askQuestion.hide();
		_logOut.hide();
	}
	_telegramFAQ.show();
}

void SettingsInner::saveError(const QString &str) {
	_errorText = str;
	resizeEvent(0);
	update();
}

void SettingsInner::supportGot(const MTPhelp_Support &support) {
	if (!App::main()) return;

	if (support.type() == mtpc_help_support) {
		const auto &d(support.c_help_support());
		UserData *u = App::feedUsers(MTP_vector<MTPUser>(1, d.vuser));
		Ui::showPeerHistory(u, ShowAtUnreadMsgId);
		App::wnd()->hideSettings();
	}
}

void SettingsInner::onUpdatePhotoCancel() {
	if (self()) {
		App::app()->cancelPhotoUpdate(self()->id);
	}
	showAll();
	update();
}

void SettingsInner::onUpdatePhoto() {
	if (!self()) {
		return;
	}

	saveError();

	QStringList imgExtensions(cImgExtensions());
	QString filter(qsl("Image files (*") + imgExtensions.join(qsl(" *")) + qsl(");;All files (*.*)"));

	QImage img;
	QString file;
	QByteArray remoteContent;
	if (filedialogGetOpenFile(file, remoteContent, lang(lng_choose_images), filter)) {
		if (!remoteContent.isEmpty()) {
			img = App::readImage(remoteContent);
		} else {
			if (!file.isEmpty()) {
				img = App::readImage(file);
			}
		}
	} else {
		return;
	}

	if (img.isNull() || img.width() > 10 * img.height() || img.height() > 10 * img.width()) {
		saveError(lang(lng_bad_photo));
		return;
	}
	PhotoCropBox *box = new PhotoCropBox(img, self());
	connect(box, SIGNAL(closed()), this, SLOT(onPhotoUpdateStart()));
	Ui::showLayer(box);
}

void SettingsInner::onShowSessions() {
	SessionsBox *box = new SessionsBox();
	Ui::showLayer(box);
}

void SettingsInner::onClearStickers() {
	auto &recent(cGetRecentStickers());
	if (!recent.isEmpty()) {
		recent.clear();
		Local::writeUserSettings();
	}
	auto &sets(Global::RefStickerSets());
	auto it = sets.find(Stickers::CustomSetId);
	if (it != sets.cend()) {
		sets.erase(it);
		Local::writeStickers();
	}
	if (auto m = App::main()) {
		emit m->stickersUpdated();
	}
	Ui::hideLayer();
}

void SettingsInner::onSwitchModerateMode() {
	Global::SetModerateModeEnabled(!Global::ModerateModeEnabled());
	Local::writeUserSettings();
	Ui::hideLayer();
}

void SettingsInner::onAskQuestion() {
	if (!App::self()) return;

	ConfirmBox *box = new ConfirmBox(lang(lng_settings_ask_sure), lang(lng_settings_ask_ok), st::defaultBoxButton, lang(lng_settings_faq_button));
	connect(box, SIGNAL(confirmed()), this, SLOT(onAskQuestionSure()));
	connect(box, SIGNAL(cancelPressed()), this, SLOT(onTelegramFAQ()));
	Ui::showLayer(box);
}

void SettingsInner::onAskQuestionSure() {
	if (_supportGetRequest) return;
	_supportGetRequest = MTP::send(MTPhelp_GetSupport(), rpcDone(&SettingsInner::supportGot));
}

void SettingsInner::onTelegramFAQ() {
	QDesktopServices::openUrl(telegramFaqLink());
}

void SettingsInner::chooseCustomLang() {
    QString file;
    QByteArray arr;
    if (filedialogGetOpenFile(file, arr, qsl("Choose language .strings file"), qsl("Language files (*.strings)"))) {
        _testlang = QFileInfo(file).absoluteFilePath();
		LangLoaderPlain loader(_testlang, LangLoaderRequest(lng_sure_save_language, lng_cancel, lng_box_ok));
        if (loader.errors().isEmpty()) {
            LangLoaderResult result = loader.found();
            QString text = result.value(lng_sure_save_language, langOriginal(lng_sure_save_language)),
				save = result.value(lng_box_ok, langOriginal(lng_box_ok)),
				cancel = result.value(lng_cancel, langOriginal(lng_cancel));
            ConfirmBox *box = new ConfirmBox(text, save, st::defaultBoxButton, cancel);
            connect(box, SIGNAL(confirmed()), this, SLOT(onSaveTestLang()));
			Ui::showLayer(box);
        } else {
			Ui::showLayer(new InformBox("Custom lang failed :(\n\nError: " + loader.errors()));
        }
    }
}

void SettingsInner::onChangeLanguage() {
	if ((_changeLanguage.clickModifiers() & Qt::ShiftModifier) && (_changeLanguage.clickModifiers() & Qt::AltModifier)) {
        chooseCustomLang();
	} else {
		Ui::showLayer(new LanguageBox());
	}
}

void SettingsInner::onSaveTestLang() {
	cSetLangFile(_testlang);
	cSetLang(languageTest);
	Local::writeSettings();
	cSetRestarting(true);
	App::quit();
}

void SettingsInner::onUpdateLocalStorage() {
	resizeEvent(0);
	updateSize(width());
	update();
}

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
void SettingsInner::onAutoUpdate() {
	cSetAutoUpdate(!cAutoUpdate());
	Local::writeSettings();
	resizeEvent(0);
	if (cAutoUpdate()) {
		Sandbox::startUpdateCheck();
		if (_updatingState == UpdatingNone) {
			_checkNow.show();
		} else if (_updatingState == UpdatingReady) {
			_restartNow.show();
		}
	} else {
		Sandbox::stopUpdate();
		_restartNow.hide();
		_checkNow.hide();
	}
	update();
}

void SettingsInner::onCheckNow() {
	if (!cAutoUpdate()) return;

	cSetLastUpdateCheck(0);
	Sandbox::startUpdateCheck();
}
#endif

void SettingsInner::onRestartNow() {
#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	checkReadyUpdate();
	if (_updatingState == UpdatingReady) {
		cSetRestartingUpdate(true);
	} else {
		cSetRestarting(true);
		cSetRestartingToSettings(true);
	}
#else
	cSetRestarting(true);
	cSetRestartingToSettings(true);
#endif
	App::quit();
}

void SettingsInner::onPasscode() {
	PasscodeBox *box = new PasscodeBox();
	connect(box, SIGNAL(closed()), this, SLOT(passcodeChanged()));
	Ui::showLayer(box);
}

void SettingsInner::onPasscodeOff() {
	PasscodeBox *box = new PasscodeBox(true);
	connect(box, SIGNAL(closed()), this, SLOT(passcodeChanged()));
	Ui::showLayer(box);
}

void SettingsInner::onPassword() {
	PasscodeBox *box = new PasscodeBox(_newPasswordSalt, _curPasswordSalt, _hasPasswordRecovery, _curPasswordHint);
	connect(box, SIGNAL(reloadPassword()), this, SLOT(onReloadPassword()));
	Ui::showLayer(box);
}

void SettingsInner::onPasswordOff() {
	if (_curPasswordSalt.isEmpty()) {
		_passwordTurnOff.hide();

//		int32 flags = MTPDaccount_passwordInputSettings::flag_new_salt | MTPDaccount_passwordInputSettings::flag_new_password_hash | MTPDaccount_passwordInputSettings::flag_hint | MTPDaccount_passwordInputSettings::flag_email;
		MTPDaccount_passwordInputSettings::Flags flags = MTPDaccount_passwordInputSettings::Flag::f_email;
		MTPaccount_PasswordInputSettings settings(MTP_account_passwordInputSettings(MTP_flags(flags), MTP_bytes(QByteArray()), MTP_bytes(QByteArray()), MTP_string(QString()), MTP_string(QString())));
		MTP::send(MTPaccount_UpdatePasswordSettings(MTP_bytes(QByteArray()), settings), rpcDone(&SettingsInner::offPasswordDone), rpcFail(&SettingsInner::offPasswordFail));
	} else {
		PasscodeBox *box = new PasscodeBox(_newPasswordSalt, _curPasswordSalt, _hasPasswordRecovery, _curPasswordHint, true);
		connect(box, SIGNAL(reloadPassword()), this, SLOT(onReloadPassword()));
		Ui::showLayer(box);
	}
}

void SettingsInner::onReloadPassword(Qt::ApplicationState state) {
	if (state == Qt::ApplicationActive) {
		MTP::send(MTPaccount_GetPassword(), rpcDone(&SettingsInner::gotPassword));
	}
}

void SettingsInner::onAutoLock() {
	AutoLockBox *box = new AutoLockBox();
	connect(box, SIGNAL(closed()), this, SLOT(passcodeChanged()));
	Ui::showLayer(box);
}

#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
void SettingsInner::onConnectionType() {
	ConnectionBox *box = new ConnectionBox();
	connect(box, SIGNAL(closed()), this, SLOT(updateConnectionType()), Qt::QueuedConnection);
	Ui::showLayer(box);
}
#endif

void SettingsInner::onUsername() {
	UsernameBox *box = new UsernameBox();
	connect(box, SIGNAL(closed()), this, SLOT(usernameChanged()));
	Ui::showLayer(box);
}

void SettingsInner::onWorkmodeTray() {
	if ((!_workmodeTray.checked() || cPlatform() != dbipWindows) && !_workmodeWindow.checked()) {
		_workmodeWindow.setChecked(true);
	}
	DBIWorkMode newMode = (_workmodeTray.checked() && _workmodeWindow.checked()) ? dbiwmWindowAndTray : (_workmodeTray.checked() ? dbiwmTrayOnly : dbiwmWindowOnly);
	if (cWorkMode() != newMode && (newMode == dbiwmWindowAndTray || newMode == dbiwmTrayOnly)) {
		cSetSeenTrayTooltip(false);
	}
	cSetWorkMode(newMode);
	App::wnd()->psUpdateWorkmode();
	Local::writeSettings();
}

void SettingsInner::onWorkmodeWindow() {
	if (!_workmodeTray.checked() && !_workmodeWindow.checked()) {
		_workmodeTray.setChecked(true);
	}
	DBIWorkMode newMode = (_workmodeTray.checked() && _workmodeWindow.checked()) ? dbiwmWindowAndTray : (_workmodeTray.checked() ? dbiwmTrayOnly : dbiwmWindowOnly);
	if (cWorkMode() != newMode && (newMode == dbiwmWindowAndTray || newMode == dbiwmTrayOnly)) {
		cSetSeenTrayTooltip(false);
	}
	cSetWorkMode(newMode);
	App::wnd()->psUpdateWorkmode();
	Local::writeSettings();
}

void SettingsInner::onAutoStart() {
	_startMinimized.setDisabled(!_autoStart.checked());
	cSetAutoStart(_autoStart.checked());
	if (!_autoStart.checked() && _startMinimized.checked()) {
		psAutoStart(false);
		_startMinimized.setChecked(false);
	} else {
		psAutoStart(_autoStart.checked());
		Local::writeSettings();
	}
}

void SettingsInner::onStartMinimized() {
	cSetStartMinimized(_startMinimized.checked());
	Local::writeSettings();
}

void SettingsInner::onSendToMenu() {
	cSetSendToMenu(_sendToMenu.checked());
	psSendToMenu(_sendToMenu.checked());
	Local::writeSettings();
}

void SettingsInner::onScaleAuto() {
	DBIScale newScale = _dpiAutoScale.checked() ? dbisAuto : cEvalScale(cConfigScale());
	if (newScale == cScreenScale()) {
		if (newScale != cScale()) {
			newScale = cScale();
		} else {
			switch (newScale) {
			case dbisOne: newScale = dbisOneAndQuarter; break;
			case dbisOneAndQuarter: newScale = dbisOne; break;
			case dbisOneAndHalf: newScale = dbisOneAndQuarter; break;
			case dbisTwo: newScale = dbisOneAndHalf; break;
			}
		}
	}
	setScale(newScale);
}

void SettingsInner::onScaleChange() {
	DBIScale newScale = dbisAuto;
	switch (_dpiSlider.selected()) {
	case 0: newScale = dbisOne; break;
	case 1: newScale = dbisOneAndQuarter; break;
	case 2: newScale = dbisOneAndHalf; break;
	case 3: newScale = dbisTwo; break;
	}
	if (newScale == cScreenScale()) {
		newScale = dbisAuto;
	}
	setScale(newScale);
}

void SettingsInner::setScale(DBIScale newScale) {
	if (cConfigScale() == newScale) return;

	cSetConfigScale(newScale);
	Local::writeSettings();
	App::wnd()->getTitle()->showUpdateBtn();
	if (newScale == dbisAuto && !_dpiAutoScale.checked()) {
		_dpiAutoScale.setChecked(true);
	} else if (newScale != dbisAuto && _dpiAutoScale.checked()) {
		_dpiAutoScale.setChecked(false);
	}
	if (newScale == dbisAuto) newScale = cScreenScale();
	if (_dpiSlider.selected() != newScale - 1) {
		_dpiSlider.setSelected(newScale - 1);
	}
	if (cEvalScale(cConfigScale()) != cEvalScale(cRealScale())) {
		ConfirmBox *box = new ConfirmBox(lang(lng_settings_need_restart), lang(lng_settings_restart_now), st::defaultBoxButton, lang(lng_settings_restart_later));
		connect(box, SIGNAL(confirmed()), this, SLOT(onRestartNow()));
		Ui::showLayer(box);
	}
}

void SettingsInner::onSoundNotify() {
	cSetSoundNotify(_soundNotify.checked());
	Local::writeUserSettings();
}

void SettingsInner::onIncludeMuted() {
	cSetIncludeMuted(_includeMuted.checked());
	Notify::unreadCounterUpdated();
	Local::writeUserSettings();
}

void SettingsInner::onWindowsNotifications() {
	if (cPlatform() != dbipWindows) return;
	cSetWindowsNotifications(!cWindowsNotifications());
	App::wnd()->notifyClearFast();
	cSetCustomNotifies(!cWindowsNotifications());
	Local::writeUserSettings();
}

void SettingsInner::onDesktopNotify() {
	cSetDesktopNotify(_desktopNotify.checked());
	if (!_desktopNotify.checked()) {
		App::wnd()->notifyClear();
		_senderName.setDisabled(true);
		_messagePreview.setDisabled(true);
	} else {
		_senderName.setDisabled(false);
		_messagePreview.setDisabled(!_senderName.checked());
	}
	Local::writeUserSettings();
	if (App::wnd()) App::wnd()->updateTrayMenu();
}

void SettingsInner::enableDisplayNotify(bool enable)
{
	_desktopNotify.setChecked(enable);
}

void SettingsInner::onSenderName() {
	_messagePreview.setDisabled(!_senderName.checked());
	if (!_senderName.checked() && _messagePreview.checked()) {
		_messagePreview.setChecked(false);
	} else {
		if (_messagePreview.checked()) {
			cSetNotifyView(dbinvShowPreview);
		} else if (_senderName.checked()) {
			cSetNotifyView(dbinvShowName);
		} else {
			cSetNotifyView(dbinvShowNothing);
		}
		Local::writeUserSettings();
		App::wnd()->notifyUpdateAll();
	}
}

void SettingsInner::onMessagePreview() {
	if (_messagePreview.checked()) {
		cSetNotifyView(dbinvShowPreview);
	} else if (_senderName.checked()) {
		cSetNotifyView(dbinvShowName);
	} else {
		cSetNotifyView(dbinvShowNothing);
	}
	Local::writeUserSettings();
	App::wnd()->notifyUpdateAll();
}

void SettingsInner::onReplaceEmojis() {
	cSetReplaceEmojis(_replaceEmojis.checked());
	Local::writeUserSettings();

	if (_replaceEmojis.checked()) {
		_viewEmojis.show();
	} else {
		_viewEmojis.hide();
	}
}

void SettingsInner::onViewEmojis() {
	Ui::showLayer(new EmojiBox());
}

void SettingsInner::onStickers() {
	Ui::showLayer(new StickersBox());
}

void SettingsInner::onEnterSend() {
	if (_enterSend.checked()) {
		cSetCtrlEnter(false);
		if (App::main()) App::main()->ctrlEnterSubmitUpdated();
		Local::writeUserSettings();
	}
}

void SettingsInner::onCtrlEnterSend() {
	if (_ctrlEnterSend.checked()) {
		cSetCtrlEnter(true);
		if (App::main()) App::main()->ctrlEnterSubmitUpdated();
		Local::writeUserSettings();
	}
}

void SettingsInner::onBackFromGallery() {
	BackgroundBox *box = new BackgroundBox();
	Ui::showLayer(box);
}

void SettingsInner::onBackFromFile() {
	QStringList imgExtensions(cImgExtensions());
	QString filter(qsl("Image files (*") + imgExtensions.join(qsl(" *")) + qsl(");;All files (*.*)"));

	QImage img;
	QString file;
	QByteArray remoteContent;
	if (filedialogGetOpenFile(file, remoteContent, lang(lng_choose_images), filter)) {
		if (!remoteContent.isEmpty()) {
			img = App::readImage(remoteContent);
		} else {
			if (!file.isEmpty()) {
				img = App::readImage(file);
			}
		}
	}

	if (img.isNull() || img.width() <= 0 || img.height() <= 0) return;

	if (img.width() > 4096 * img.height()) {
		img = img.copy((img.width() - 4096 * img.height()) / 2, 0, 4096 * img.height(), img.height());
	} else if (img.height() > 4096 * img.width()) {
		img = img.copy(0, (img.height() - 4096 * img.width()) / 2, img.width(), 4096 * img.width());
	}

	App::initBackground(-1, img);
	_tileBackground.setChecked(false);
	updateChatBackground();
}

float64 SettingsInner::radialProgress() const {
	if (auto m = App::main()) {
		return m->chatBackgroundProgress();
	}
	return 1.;
}

bool SettingsInner::radialLoading() const {
	if (auto m = App::main()) {
		if (m->chatBackgroundLoading()) {
			m->checkChatBackground();
			if (m->chatBackgroundLoading()) {
				return true;
			} else {
				const_cast<SettingsInner*>(this)->updateChatBackground();
			}
		}
	}
	return false;
}

QRect SettingsInner::radialRect() const {
	auto left = _left;
	auto top = _tileBackground.y() - st::setLittleSkip - st::setBackgroundSize;
	return QRect(left, top, st::setBackgroundSize, st::setBackgroundSize);
}

void SettingsInner::radialStart() {
	if (radialLoading() && !_radial.animating()) {
		_radial.start(radialProgress());
		if (auto shift = radialTimeShift()) {
			_radial.update(radialProgress(), !radialLoading(), getms() + shift);
		}
	}
}

uint64 SettingsInner::radialTimeShift() const {
	return st::radialDuration;
}

void SettingsInner::step_radial(uint64 ms, bool timer) {
	_radial.update(radialProgress(), !radialLoading(), ms + radialTimeShift());
	if (timer && _radial.animating()) {
		updateBackgroundRect();
	}
}

void SettingsInner::updateChatBackground() {
	int32 size = st::setBackgroundSize * cIntRetinaFactor();
	QImage back(size, size, QImage::Format_ARGB32_Premultiplied);
	back.setDevicePixelRatio(cRetinaFactor());
	{
		QPainter p(&back);
		const QPixmap &pix(*cChatBackground());
		int sx = (pix.width() > pix.height()) ? ((pix.width() - pix.height()) / 2) : 0;
		int sy = (pix.height() > pix.width()) ? ((pix.height() - pix.width()) / 2) : 0;
		int s = (pix.width() > pix.height()) ? pix.height() : pix.width();
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		p.drawPixmap(0, 0, st::setBackgroundSize, st::setBackgroundSize, pix, sx, sy, s, s);
	}
	_background = QPixmap::fromImage(back);
	_background.setDevicePixelRatio(cRetinaFactor());
	_needBackgroundUpdate = false;

	updateBackgroundRect();
}

void SettingsInner::needBackgroundUpdate(bool tile) {
	_needBackgroundUpdate = true;
	_tileBackground.setChecked(tile);
	updateChatBackground();
	if (radialLoading()) {
		radialStart();
	}
}

void SettingsInner::onTileBackground() {
	if (cTileBackground() != _tileBackground.checked()) {
		cSetTileBackground(_tileBackground.checked());
		if (App::main()) App::main()->clearCachedBackground();
		Local::writeUserSettings();
	}
}

void SettingsInner::onAdaptiveForWide() {
	if (Global::AdaptiveForWide() != _adaptiveForWide.checked()) {
		Global::SetAdaptiveForWide(_adaptiveForWide.checked());
		if (App::wnd()) {
			App::wnd()->updateAdaptiveLayout();
		}
		Local::writeUserSettings();
	}
}

void SettingsInner::onDontAskDownloadPath() {
	cSetAskDownloadPath(!_dontAskDownloadPath.checked());
	Local::writeUserSettings();

	showAll();
	resizeEvent(0);
	update();
}

void SettingsInner::onDownloadPathEdit() {
	DownloadPathBox *box = new DownloadPathBox();
	connect(box, SIGNAL(closed()), this, SLOT(onDownloadPathEdited()));
	Ui::showLayer(box);
}

void SettingsInner::onDownloadPathEdited() {
	QString path;
	if (cDownloadPath().isEmpty()) {
		path = lang(lng_download_path_default);
	} else if (cDownloadPath() == qsl("tmp")) {
		path = lang(lng_download_path_temp);
	} else {
		path = st::linkFont->elided(QDir::toNativeSeparators(cDownloadPath()), st::setWidth - st::setVersionLeft - _downloadPathWidth);
	}
	_downloadPathEdit.setText(path);
	showAll();
}

void SettingsInner::onDownloadPathClear() {
	ConfirmBox *box = new ConfirmBox(lang(lng_sure_clear_downloads));
	connect(box, SIGNAL(confirmed()), this, SLOT(onDownloadPathClearSure()));
	Ui::showLayer(box);
}

void SettingsInner::onDownloadPathClearSure() {
	Ui::hideLayer();
	App::wnd()->tempDirDelete(Local::ClearManagerDownloads);
	_tempDirClearState = TempDirClearing;
	showAll();
	update();
}

void SettingsInner::onLocalStorageClear() {
	App::wnd()->tempDirDelete(Local::ClearManagerStorage);
	_storageClearState = TempDirClearing;
	showAll();
	update();
}

void SettingsInner::onTempDirCleared(int task) {
	if (task & Local::ClearManagerDownloads) {
		_tempDirClearState = TempDirCleared;
	} else if (task & Local::ClearManagerStorage) {
		_storageClearState = TempDirCleared;
	}
	showAll();
	update();
}

void SettingsInner::onTempDirClearFailed(int task) {
	if (task & Local::ClearManagerDownloads) {
		_tempDirClearState = TempDirClearFailed;
	} else if (task & Local::ClearManagerStorage) {
		_storageClearState = TempDirClearFailed;
	}
	showAll();
	update();
}

void SettingsInner::onAutoDownload() {
	Ui::showLayer(new AutoDownloadBox());
}

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
void SettingsInner::setUpdatingState(UpdatingState state, bool force) {
	if (_updatingState != state || force) {
		_updatingState = state;
		if (cAutoUpdate()) {
			switch (state) {
			case UpdatingNone:
			case UpdatingLatest: _checkNow.show(); _restartNow.hide(); break;
			case UpdatingReady: _checkNow.hide(); _restartNow.show(); break;
			case UpdatingCheck:
			case UpdatingDownload:
			case UpdatingFail: _checkNow.hide(); _restartNow.hide(); break;
			}
			update(0, _restartNow.y() - 10, width(), _restartNow.height() + 20);
		} else {
			_checkNow.hide();
			_restartNow.hide();
		}
	}
}

void SettingsInner::setDownloadProgress(qint64 ready, qint64 total) {
	qint64 readyTenthMb = (ready * 10 / (1024 * 1024)), totalTenthMb = (total * 10 / (1024 * 1024));
	QString readyStr = QString::number(readyTenthMb / 10) + '.' + QString::number(readyTenthMb % 10);
	QString totalStr = QString::number(totalTenthMb / 10) + '.' + QString::number(totalTenthMb % 10);
	QString res = lng_settings_downloading(lt_ready, readyStr, lt_total, totalStr);
	if (_newVersionDownload != res) {
		_newVersionDownload = res;
		if (cAutoUpdate()) {
			update(0, _restartNow.y() - 10, width(), _restartNow.height() + 20);
		}
	}
}

void SettingsInner::onUpdateChecking() {
	setUpdatingState(UpdatingCheck);
}

void SettingsInner::onUpdateLatest() {
	setUpdatingState(UpdatingLatest);
}

void SettingsInner::onUpdateDownloading(qint64 ready, qint64 total) {
	setUpdatingState(UpdatingDownload);
	setDownloadProgress(ready, total);
}

void SettingsInner::onUpdateReady() {
	setUpdatingState(UpdatingReady);
}

void SettingsInner::onUpdateFailed() {
	setUpdatingState(UpdatingFail);
}
#endif

void SettingsInner::onPhotoUpdateStart() {
	showAll();
	update();
}

void SettingsInner::onPhotoUpdateFail(PeerId peer) {
	if (!self() || self()->id != peer) return;
	saveError(lang(lng_bad_photo));
	showAll();
	update();
}

void SettingsInner::onPhotoUpdateDone(PeerId peer) {
	if (!self() || self()->id != peer) return;
	showAll();
	update();
}

SettingsWidget::SettingsWidget(MainWindow *parent) : TWidget(parent)
, _a_show(animation(this, &SettingsWidget::step_show))
, _scroll(this, st::setScroll)
, _inner(this)
, _close(this, st::setClose) {
	_scroll.setWidget(&_inner);

	connect(App::wnd(), SIGNAL(resized(const QSize&)), this, SLOT(onParentResize(const QSize&)));
	connect(&_close, SIGNAL(clicked()), App::wnd(), SLOT(showSettings()));

	setGeometry(QRect(0, st::titleHeight, App::wnd()->width(), App::wnd()->height() - st::titleHeight));

	showAll();
}

void SettingsWidget::onParentResize(const QSize &newSize) {
	resize(newSize);
}

void SettingsWidget::animShow(const QPixmap &bgAnimCache, bool back) {
	if (App::app()) App::app()->mtpPause();

	(back ? _cacheOver : _cacheUnder) = bgAnimCache;

	_a_show.stop();

	showAll();
	(back ? _cacheUnder : _cacheOver) = myGrab(this);
	hideAll();

	a_coordUnder = back ? anim::ivalue(-st::slideShift, 0) : anim::ivalue(0, -st::slideShift);
	a_coordOver = back ? anim::ivalue(0, width()) : anim::ivalue(width(), 0);
	a_shadow = back ? anim::fvalue(1, 0) : anim::fvalue(0, 1);
	_a_show.start();

	show();
}

void SettingsWidget::step_show(float64 ms, bool timer) {
	float64 dt = ms / st::slideDuration;
	if (dt >= 1) {
		_a_show.stop();

		a_coordUnder.finish();
		a_coordOver.finish();
		a_shadow.finish();

		_cacheUnder = _cacheOver = QPixmap();

		showAll();
		_inner.setFocus();

		if (App::app()) App::app()->mtpUnpause();
	} else {
		a_coordUnder.update(dt, st::slideFunction);
		a_coordOver.update(dt, st::slideFunction);
		a_shadow.update(dt, st::slideFunction);
	}
	if (timer) update();
}

void SettingsWidget::stop_show() {
	_a_show.stop();
}

void SettingsWidget::paintEvent(QPaintEvent *e) {
	QRect r(e->rect());
	bool trivial = (rect() == r);

	Painter p(this);
	if (!trivial) {
		p.setClipRect(r);
	}
	if (_a_show.animating()) {
		if (a_coordOver.current() > 0) {
			p.drawPixmap(QRect(0, 0, a_coordOver.current(), height()), _cacheUnder, QRect(-a_coordUnder.current() * cRetinaFactor(), 0, a_coordOver.current() * cRetinaFactor(), height() * cRetinaFactor()));
			p.setOpacity(a_shadow.current() * st::slideFadeOut);
			p.fillRect(0, 0, a_coordOver.current(), height(), st::black->b);
			p.setOpacity(1);
		}
		p.drawPixmap(a_coordOver.current(), 0, _cacheOver);
		p.setOpacity(a_shadow.current());
		p.drawPixmap(QRect(a_coordOver.current() - st::slideShadow.pxWidth(), 0, st::slideShadow.pxWidth(), height()), App::sprite(), st::slideShadow.rect());
	} else {
		p.fillRect(rect(), st::setBG->b);
	}
}

void SettingsWidget::showAll() {
	_scroll.show();
	_inner.show();
	_inner.showAll();
	if (Adaptive::OneColumn()) {
		_close.hide();
	} else {
		_close.show();
	}
}

void SettingsWidget::hideAll() {
	_scroll.hide();
	_close.hide();
}

void SettingsWidget::resizeEvent(QResizeEvent *e) {
	_scroll.resize(size());
	_inner.updateSize(width());
	_close.move(st::setClosePos.x(), st::setClosePos.y());
}

void SettingsWidget::dragEnterEvent(QDragEnterEvent *e) {

}

void SettingsWidget::dropEvent(QDropEvent *e) {
}

void SettingsWidget::updateAdaptiveLayout() {
	if (Adaptive::OneColumn()) {
		_close.hide();
	} else {
		_close.show();
	}
	_inner.updateAdaptiveLayout();
	resizeEvent(0);
}

void SettingsWidget::updateDisplayNotify() {
	_inner.enableDisplayNotify(cDesktopNotify());
}

void SettingsWidget::updateOnlineDisplay() {
	_inner.updateOnlineDisplay();
}

void SettingsWidget::updateConnectionType() {
	_inner.updateConnectionType();
}

void SettingsWidget::rpcClear() {
	_inner.rpcClear();
}

void SettingsWidget::setInnerFocus() {
	_inner.setFocus();
}

void SettingsWidget::needBackgroundUpdate(bool tile) {
	_inner.needBackgroundUpdate(tile);
}

SettingsWidget::~SettingsWidget() {
	if (App::wnd()) App::wnd()->noSettings(this);
}
