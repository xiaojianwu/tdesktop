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
#pragma once

#include "abstractbox.h"

namespace Dialogs {
class Row;
class IndexedList;
} // namespace Dialogs

enum MembersFilter {
	MembersFilterRecent,
	MembersFilterAdmins,
};
using MembersAlreadyIn = OrderedSet<UserData*>;

QString cantInviteError();

class ConfirmBox;
class ContactsInner : public TWidget, public RPCSender {
	Q_OBJECT

private:

	struct ContactData;

public:

	ContactsInner(CreatingGroupType creating = CreatingGroupNone);
	ContactsInner(ChannelData *channel, MembersFilter membersFilter, const MembersAlreadyIn &already);
	ContactsInner(ChatData *chat, MembersFilter membersFilter);
	ContactsInner(UserData *bot);
	void init();
	void initList();

	void paintEvent(QPaintEvent *e);
	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void resizeEvent(QResizeEvent *e);

	void paintDialog(Painter &p, PeerData *peer, ContactData *data, bool sel);
	void updateFilter(QString filter = QString());

	void selectSkip(int32 dir);
	void selectSkipPage(int32 h, int32 dir);

	QVector<UserData*> selected();
	QVector<MTPInputUser> selectedInputs();
	PeerData *selectedUser();
	bool allAdmins() const {
		return _allAdmins.checked();
	}

	void loadProfilePhotos(int32 yFrom);
	void chooseParticipant();
	void changeCheckState(Dialogs::Row *row);
	void changeCheckState(ContactData *data, PeerData *peer);

	void peopleReceived(const QString &query, const QVector<MTPPeer> &people);

	void refresh();

	ChatData *chat() const;
	ChannelData *channel() const;
	MembersFilter membersFilter() const;
	UserData *bot() const;
	CreatingGroupType creating() const;

	int32 selectedCount() const;
	bool hasAlreadyMembersInChannel() const {
		return !_already.isEmpty();
	}

	void saving(bool flag);

	~ContactsInner();

signals:

	void mustScrollTo(int ymin, int ymax);
	void selectAllQuery();
	void searchByUsername();
	void chosenChanged();
	void adminAdded();
	void addRequested();

public slots:

	void onDialogRowReplaced(Dialogs::Row *oldRow, Dialogs::Row *newRow);

	void updateSel();
	void peerUpdated(PeerData *peer);
	void onPeerNameChanged(PeerData *peer, const PeerData::Names &oldNames, const PeerData::NameFirstChars &oldChars);

	void onAddBot();
	void onAddAdmin();
	void onNoAddAdminBox(QObject *obj);

	void onAllAdminsChanged();

private:

	void updateSelectedRow();
	void addAdminDone(const MTPUpdates &result, mtpRequestId req);
	bool addAdminFail(const RPCError &error, mtpRequestId req);

	int32 _rowHeight;
	int _newItemHeight = 0;
	bool _newItemSel = false;

	ChatData *_chat = nullptr;
	ChannelData *_channel = nullptr;
	MembersFilter _membersFilter = MembersFilterRecent;
	UserData *_bot = nullptr;
	CreatingGroupType _creating = CreatingGroupNone;
	MembersAlreadyIn _already;

	Checkbox _allAdmins;
	int32 _aboutWidth;
	Text _aboutAllAdmins, _aboutAdmins;

	PeerData *_addToPeer = nullptr;
	UserData *_addAdmin = nullptr;
	mtpRequestId _addAdminRequestId = 0;
	ConfirmBox *_addAdminBox = nullptr;

	int32 _time;

	std_::unique_ptr<Dialogs::IndexedList> _customList;
	Dialogs::IndexedList *_contacts = nullptr;
	Dialogs::Row *_sel = nullptr;
	QString _filter;
	typedef QVector<Dialogs::Row*> FilteredDialogs;
	FilteredDialogs _filtered;
	int _filteredSel = -1;
	bool _mouseSel = false;

	int _selCount = 0;

	struct ContactData {
		Text name;
		QString online;
		bool onlineColor;
		bool inchat;
		bool check;
	};
	typedef QMap<PeerData*, ContactData*> ContactsData;
	ContactsData _contactsData;
	typedef QMap<PeerData*, bool> CheckedContacts;
	CheckedContacts _checkedContacts;

	ContactData *contactData(Dialogs::Row *row);

	bool _searching = false;
	QString _lastQuery;
	typedef QVector<PeerData*> ByUsernameRows;
	typedef QVector<ContactData*> ByUsernameDatas;
	ByUsernameRows _byUsername, _byUsernameFiltered;
	ByUsernameDatas d_byUsername, d_byUsernameFiltered; // filtered is partly subset of d_byUsername, partly subset of _byUsernameDatas
	ByUsernameDatas _byUsernameDatas;
	int _byUsernameSel = -1;

	QPoint _lastMousePos;
	LinkButton _addContactLnk;

	bool _saving = false;
	bool _allAdminsChecked = false;

};

class ContactsBox : public ItemListBox, public RPCSender {
	Q_OBJECT

public:

	ContactsBox();
	ContactsBox(const QString &name, const QImage &photo); // group creation
	ContactsBox(ChannelData *channel); // channel setup
	ContactsBox(ChannelData *channel, MembersFilter filter, const MembersAlreadyIn &already);
	ContactsBox(ChatData *chat, MembersFilter filter);
	ContactsBox(UserData *bot);
	void keyPressEvent(QKeyEvent *e);
	void paintEvent(QPaintEvent *e);
	void resizeEvent(QResizeEvent *e);

	void closePressed();

	void setInnerFocus() {
		_filter.setFocus();
	}

signals:

	void adminAdded();

public slots:

	void onFilterUpdate();
	void onFilterCancel();
	void onChosenChanged();
	void onScroll();

	void onInvite();
	void onCreate();
	void onSaveAdmins();

	void onSubmit();

	bool onSearchByUsername(bool searchCache = false);
	void onNeedSearchByUsername();

protected:

	void hideAll();
	void showAll();
	void showDone();

private:

	void init();

	ContactsInner _inner;
	InputField _filter;
	IconedButton _filterCancel;

	BoxButton _next, _cancel;
	MembersFilter _membersFilter;

	ScrollableBoxShadow _topShadow, *_bottomShadow;

	void peopleReceived(const MTPcontacts_Found &result, mtpRequestId req);
	bool peopleFailed(const RPCError &error, mtpRequestId req);

	QTimer _searchTimer;
	QString _peopleQuery;
	bool _peopleFull;
	mtpRequestId _peopleRequest;

	typedef QMap<QString, MTPcontacts_Found> PeopleCache;
	PeopleCache _peopleCache;

	typedef QMap<mtpRequestId, QString> PeopleQueries;
	PeopleQueries _peopleQueries;

	int32 _saveRequestId;

	// saving admins
	void saveAdminsDone(const MTPUpdates &result);
	void saveSelectedAdmins();
	void getAdminsDone(const MTPmessages_ChatFull &result);
	void setAdminDone(UserData *user, const MTPBool &result);
	void removeAdminDone(UserData *user, const MTPBool &result);
	bool saveAdminsFail(const RPCError &error);
	bool editAdminFail(const RPCError &error);

	// group creation
	QString _creationName;
	QImage _creationPhoto;

	void creationDone(const MTPUpdates &updates);
	bool creationFail(const RPCError &e);
};

class MembersInner : public TWidget, public RPCSender {
	Q_OBJECT

private:

	struct MemberData;

public:

	MembersInner(ChannelData *channel, MembersFilter filter);

	void paintEvent(QPaintEvent *e);
	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);

	void paintDialog(Painter &p, PeerData *peer, MemberData *data, bool sel, bool kickSel, bool kickDown);

	void selectSkip(int32 dir);
	void selectSkipPage(int32 h, int32 dir);

	void loadProfilePhotos(int32 yFrom);
	void chooseParticipant();

	void refresh();

	ChannelData *channel() const;
	MembersFilter filter() const;

	bool isLoaded() const {
		return !_loading;
	}
	void clearSel();

	MembersAlreadyIn already() const;

	~MembersInner();

signals:

	void mustScrollTo(int ymin, int ymax);
	void addRequested();
	void loaded();

public slots:

	void load();

	void updateSel();
	void peerUpdated(PeerData *peer);
	void onPeerNameChanged(PeerData *peer, const PeerData::Names &oldNames, const PeerData::NameFirstChars &oldChars);
	void onKickConfirm();
	void onKickBoxDestroyed(QObject *obj);

private:

	void updateSelectedRow();
	MemberData *data(int32 index);

	void membersReceived(const MTPchannels_ChannelParticipants &result, mtpRequestId req);
	bool membersFailed(const RPCError &error, mtpRequestId req);

	void kickDone(const MTPUpdates &result, mtpRequestId req);
	void kickAdminDone(const MTPUpdates &result, mtpRequestId req);
	bool kickFail(const RPCError &error, mtpRequestId req);
	void removeKicked();

	void clear();

	int32 _rowHeight, _newItemHeight;
	bool _newItemSel;

	ChannelData *_channel;
	MembersFilter _filter;

	QString _kickText;
	int32 _time, _kickWidth;

	int32 _sel, _kickSel, _kickDown;
	bool _mouseSel;

	UserData *_kickConfirm;
	mtpRequestId _kickRequestId;

	ConfirmBox *_kickBox;

	enum MemberRole {
		MemberRoleNone,
		MemberRoleSelf,
		MemberRoleCreator,
		MemberRoleEditor,
		MemberRoleModerator,
		MemberRoleKicked
	};

	struct MemberData {
		Text name;
		QString online;
		bool onlineColor;
		bool canKick;
	};

	bool _loading;
	mtpRequestId _loadingRequestId;
	typedef QVector<UserData*> MemberRows;
	typedef QVector<QDateTime> MemberDates;
	typedef QVector<MemberRole> MemberRoles;
	typedef QVector<MemberData*> MemberDatas;
	MemberRows _rows;
	MemberDates _dates;
	MemberRoles _roles;
	MemberDatas _datas;

	int32 _aboutWidth;
	Text _about;
	int32 _aboutHeight;

	QPoint _lastMousePos;

};

class MembersBox : public ItemListBox {
	Q_OBJECT

public:

	MembersBox(ChannelData *channel, MembersFilter filter);
	void keyPressEvent(QKeyEvent *e);
	void paintEvent(QPaintEvent *e);
	void resizeEvent(QResizeEvent *e);

	void setInnerFocus() {
		setFocus();
	}

public slots:

	void onScroll();

	void onAdd();
	void onAdminAdded();

protected:

	void showDone();

private:

	MembersInner _inner;

	ContactsBox *_addBox;

	SingleTimer _loadTimer;
};
