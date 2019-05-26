// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "syncdatadb.h"
#include "commons/serialize.h"
#include <map>

namespace SyncData {

boost::shared_ptr<CLevelDBWrapper> CSyncDataDb::m_dbPoint;

bool CSyncDataDb::InitializeSyncDataDb(const boost::filesystem::path &path)
{
	if (!m_dbPoint)
	{
		m_dbPoint.reset(new CLevelDBWrapper(path, 0));
	}
	return (NULL != m_dbPoint.get());
}

bool CSyncDataDb::WriteCheckpoint(int height, const CSyncData& data)
{
	bool ret = false;
	if (m_dbPoint)
	{
		ret = m_dbPoint->Write(std::make_pair('c', height), data);
	}
	return ret;
}

bool CSyncDataDb::ReadCheckpoint(int height, CSyncData& data)
{
	bool ret = false;
	if (m_dbPoint)
	{
		ret = m_dbPoint->Read(std::make_pair('c', height), data);
	}
	return ret;
}

bool CSyncDataDb::ExistCheckpoint(int height)
{
	bool ret = false;
	if (m_dbPoint)
	{
		ret = m_dbPoint->Exists(height);
	}
	return ret;
}

bool CSyncDataDb::LoadCheckPoint(std::map<int, uint256>& values)
{
	bool ret = false;
	if (m_dbPoint)
	{
		leveldb::Iterator *pcursor = m_dbPoint->NewIterator();
		CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
		ssKeySet << std::make_pair('c', 0);
		pcursor->Seek(ssKeySet.str());

		while (pcursor->Valid())
		{
			try
			{
				leveldb::Slice slKey = pcursor->key();
				CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
				char chType = 0;
				int height = 0;
				ssKey >> chType;
				if (chType == 'c')
				{
					ssKey >> height;
					leveldb::Slice slValue = pcursor->value();
					CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
					CSyncData data;
					ssValue >> data;
					CSyncCheckPoint point;
					point.SetData(data);
					values.insert(std::make_pair(height, point.m_hashCheckpoint));
					pcursor->Next();
				}
				else
				{
					break;
				}
			}
			catch (std::exception &e)
			{
				return ERRORMSG("%s : Deserialize or I/O error - %s", __PRETTY_FUNCTION__, e.what());
			}
		}
		delete pcursor;
		ret = true;
	}
	return ret;
}


} /* namespace SyncData */