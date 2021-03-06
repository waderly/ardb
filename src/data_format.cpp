/*
 *Copyright (c) 2013-2013, yinqiwen <yinqiwen@gmail.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Redis nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * data_format.cpp
 *
 *  Created on: 2013/12/24
 *      Author: wqy
 */
#include "data_format.hpp"
#include "geo/geohash_helper.hpp"
#include <cmath>

#define  GET_KEY_TYPE(KEY, TYPE)   do{ \
		Iterator* iter = FindValue(KEY, true);  \
		if (NULL != iter && iter->Valid()) \
		{                                  \
			Slice tmp = iter->Key();       \
			KeyObject* k = decode_key(tmp, &KEY); \
			if(NULL != k && k->key.compare(KEY.key) == 0)\
			{                                            \
				TYPE = k->type;                          \
	        }                                            \
	        DELETE(k);                                   \
	    }\
	    DELETE(iter);\
}while(0)

namespace ardb
{
    std::string ValueData::AsString() const
    {
        std::string str;
        ToString(str);
        return str;
    }

    std::string& ValueData::ToString(std::string& str) const
    {
        switch (type)
        {
            case INTEGER_VALUE:
            {
                char tmp[100];
                sprintf(tmp, "%"PRId64, integer_value);
                str = tmp;
                break;
            }
            case DOUBLE_VALUE:
            {
                //fast_dtoa(double_value, 10, str);
                if (std::isinf(double_value))
                {
                    /* Libc in odd systems (Hi Solaris!) will format infinite in a
                     * different way, so better to handle it in an explicit way. */
                    if(double_value > 0)
                    {
                        str = "inf";
                    }else
                    {
                        str = "-inf";
                    }
                }
                else
                {
                    char dbuf[128];
                    int dlen = snprintf(dbuf, sizeof(dbuf), "%.17g", double_value);
                    str.assign(dbuf, dlen);
                }
                break;
            }
            case BYTES_VALUE:
            {
                str = bytes_value;
                break;
            }
            default:
            {
                break;
            }
        }
        return str;
    }

    bool ValueData::Encode(Buffer& buf) const
    {
        BufferHelper::WriteFixUInt8(buf, type);
        switch (type)
        {
            case DOUBLE_VALUE:
            {
                BufferHelper::WriteFixDouble(buf, double_value);
                break;
            }
            case INTEGER_VALUE:
            {
                BufferHelper::WriteVarInt64(buf, integer_value);
                break;
            }
            case BYTES_VALUE:
            {
                BufferHelper::WriteVarString(buf, bytes_value);
                break;
            }
            default:
            {
                break;
            }
        }
        return true;
    }
    bool ValueData::Decode(Buffer& buf, bool to_slice)
    {
        if (!BufferHelper::ReadFixUInt8(buf, type))
        {
            return false;
        }
        bool ret = false;
        switch (type)
        {
            case DOUBLE_VALUE:
            {
                ret = BufferHelper::ReadFixDouble(buf, double_value);
                break;
            }
            case INTEGER_VALUE:
            {
                ret = BufferHelper::ReadVarInt64(buf, integer_value);
                break;
            }
            case BYTES_VALUE:
            {
                if (to_slice)
                {
                    ret = BufferHelper::ReadVarSlice(buf, slice_value);
                }
                else
                {
                    ret = BufferHelper::ReadVarString(buf, bytes_value);
                }
                break;
            }
            case EMPTY_VALUE:
            {
                ret = true;
                break;
            }
            default:
            {
                break;
            }
        }
        return ret;
    }

    int ValueData::IncrbyFloat(double value)
    {
        if (type == BYTES_VALUE)
        {
            return -1;
        }
        if (type == EMPTY_VALUE || type == DOUBLE_VALUE)
        {
            type = DOUBLE_VALUE;
            double_value += value;
            return 0;
        }
        if (type == INTEGER_VALUE)
        {
            type = DOUBLE_VALUE;
            double_value = (integer_value + value);
            return 0;
        }
        return -1;
    }

    int ValueData::Incrby(int64 value)
    {
        if (type == BYTES_VALUE)
        {
            return -1;
        }
        if (type == EMPTY_VALUE || type == INTEGER_VALUE)
        {
            type = INTEGER_VALUE;
            integer_value += value;
            return 0;
        }
        if (type == DOUBLE_VALUE)
        {
            double_value += value;
            return 0;
        }
        return -1;
    }
    ValueData& ValueData::operator+=(const ValueData& other)
    {
        if (type == EMPTY_VALUE)
        {
            type = INTEGER_VALUE;
            integer_value = 0;
        }
        if (other.type == DOUBLE_VALUE || type == DOUBLE_VALUE)
        {
            double tmp = NumberValue();
            tmp += other.NumberValue();
            type = DOUBLE_VALUE;
            double_value = tmp;
            return *this;
        }
        if ((other.type == INTEGER_VALUE || other.type == EMPTY_VALUE) && type == INTEGER_VALUE)
        {
            integer_value += other.integer_value;
            return *this;
        }
        return *this;
    }
    ValueData& ValueData::operator/=(uint32 i)
    {
        if (type == EMPTY_VALUE)
        {
            type = INTEGER_VALUE;
            integer_value = 0;
        }
        if (type == BYTES_VALUE)
        {
            return *this;
        }
        double tmp = NumberValue();
        type = DOUBLE_VALUE;
        if (i != 0)
        {
            double_value = tmp / i;
        }
        else
        {
            double_value = DBL_MAX;
        }
        return *this;
    }

    double ValueData::NumberValue() const
    {
        switch (type)
        {
            case INTEGER_VALUE:
            {
                return integer_value;
            }
            case DOUBLE_VALUE:
            {
                return double_value;
            }
            default:
            {
                return NAN;
            }
        }
    }

    int ValueData::ToBytes()
    {
        if (type == EMPTY_VALUE)
        {
            return -1;
        }
        ToString(bytes_value);
        return -1;
    }

    int ValueData::ToNumber()
    {
        if (type == EMPTY_VALUE)
        {
            return -1;
        }
        if (type != BYTES_VALUE)
        {
            return 0;
        }
        if (raw_toint64(bytes_value.data(), bytes_value.size(), integer_value))
        {
            bytes_value.clear();
            type = INTEGER_VALUE;
            return 1;
        }
        else if (raw_todouble(bytes_value.data(), bytes_value.size(), double_value))
        {
            bytes_value.clear();
            type = DOUBLE_VALUE;
            return 1;
        }
        return -1;
    }

    void ValueData::SetValue(const Slice& value, bool auto_convert)
    {
        if (!auto_convert)
        {
            type = BYTES_VALUE;
            bytes_value.assign(value.data(), value.size());
        }
        else
        {
            if (value.empty())
            {
                type = EMPTY_VALUE;
                return;
            }
            char first_char = value.data()[0];
            if (first_char != '+' && value.data()[0] != '-' && (first_char < '0' || first_char > '9'))
            {
                type = BYTES_VALUE;
                bytes_value.assign(value.data(), value.size());
            }
            else
            {
                if (raw_toint64(value.data(), value.size(), integer_value))
                {
                    type = INTEGER_VALUE;
                }
                else if (raw_todouble(value.data(), value.size(), double_value))
                {
                    type = DOUBLE_VALUE;
                }
                else
                {
                    type = BYTES_VALUE;
                    bytes_value.assign(value.data(), value.size());
                }
            }
        }
    }

    int ValueData::Compare(const ValueData& other) const
    {
        if (type != other.type)
        {
            return COMPARE_NUMBER(type, other.type);
        }
        switch (type)
        {
            case EMPTY_VALUE:
            {
                return 0;
            }
            case INTEGER_VALUE:
            {
                return COMPARE_NUMBER(integer_value, other.integer_value);
            }
            case DOUBLE_VALUE:
            {
                return COMPARE_NUMBER(double_value, other.double_value);
            }
            default:
            {
                if (bytes_value.empty() && other.bytes_value.empty())
                {
                    return slice_value.compare(other.slice_value);
                }
                return bytes_value.compare(other.bytes_value);
            }
        }
    }

    void ValueData::Clear()
    {
        integer_value = 0;
        double_value = 0;
        bytes_value.clear();
        type = EMPTY_VALUE;
    }

    ZSetKeyObject::ZSetKeyObject(const Slice& k, const ValueData& v, const ValueData& s, DBID id) :
            KeyObject(k, ZSET_ELEMENT, id)
    {
        score = s;
        value = v;
    }

    ZSetKeyObject::ZSetKeyObject(const Slice& k, const ZSetElement& ee, DBID id) :
            KeyObject(k, ZSET_ELEMENT, id)
    {
        score = ee.score;
        value = ee.value;
    }

    ZSetKeyObject::ZSetKeyObject(const Slice& k, const Slice& v, const ValueData& s, DBID id) :
            KeyObject(k, ZSET_ELEMENT, id)
    {
        score = s;
        value.SetValue(v, true);
    }

    ZSetNodeKeyObject::ZSetNodeKeyObject(const Slice& k, const ValueData& v, DBID id) :
            KeyObject(k, ZSET_ELEMENT_NODE, id), value(v)
    {

    }

    ZSetNodeKeyObject::ZSetNodeKeyObject(const Slice& k, const Slice& v, DBID id) :
            KeyObject(k, ZSET_ELEMENT_NODE, id)
    {
        value.SetValue(v, true);
    }

    int ardb_compare_keys(const char* akbuf, size_t aksiz, const char* bkbuf, size_t bksiz)
    {
        Buffer ak_buf(const_cast<char*>(akbuf), 0, aksiz);
        Buffer bk_buf(const_cast<char*>(bkbuf), 0, bksiz);
        uint32 aheader, bheader;
        bool found_a = BufferHelper::ReadFixUInt32(ak_buf, aheader);
        bool found_b = BufferHelper::ReadFixUInt32(bk_buf, bheader);
        COMPARE_EXIST(found_a, found_b);
        uint32 adb = aheader >> 8;
        uint32 bdb = bheader >> 8;
        RETURN_NONEQ_RESULT(adb, bdb);
        uint8 at = aheader & 0xFF;
        uint8 bt = bheader & 0xFF;
        RETURN_NONEQ_RESULT(at, bt);
        Slice akey, bkey;
        found_a = BufferHelper::ReadVarSlice(ak_buf, akey);
        found_b = BufferHelper::ReadVarSlice(bk_buf, bkey);
        COMPARE_EXIST(found_a, found_b);
        int ret = 0;
        if (at != KEY_EXPIRATION_ELEMENT)
        {
            ret = akey.compare(bkey);
        }
        if (ret != 0)
        {
            return ret;
        }
        switch (at)
        {
            case HASH_FIELD:
            {
                ValueData av, bv;
                found_a = decode_value(ak_buf, av, true);
                found_b = decode_value(bk_buf, bv, true);
                COMPARE_EXIST(found_a, found_b);
                ret = av.Compare(bv);
                break;
            }
            case LIST_ELEMENT:
            {
                ValueData av, bv;
                found_a = decode_value(ak_buf, av, true);
                found_b = decode_value(bk_buf, bv, true);
                COMPARE_EXIST(found_a, found_b);
                ret = COMPARE_NUMBER(av.NumberValue(), bv.NumberValue());
                break;
            }
            case ZSET_ELEMENT_NODE:
            case SET_ELEMENT:
            {
                ValueData av, bv;
                found_a = decode_value(ak_buf, av, true);
                found_b = decode_value(bk_buf, bv, true);
                COMPARE_EXIST(found_a, found_b);
                ret = av.Compare(bv);
                break;
            }
            case ZSET_ELEMENT:
            {
                ValueData ascore, bscore;
                found_a = decode_value(ak_buf, ascore, true);
                found_b = decode_value(bk_buf, bscore, true);
                COMPARE_EXIST(found_a, found_b);
                ret = COMPARE_NUMBER(ascore.NumberValue(), bscore.NumberValue());
                if (ret == 0)
                {
                    ValueData av, bv;
                    found_a = decode_value(ak_buf, av, true);
                    found_b = decode_value(bk_buf, bv, true);
                    COMPARE_EXIST(found_a, found_b);
                    ret = av.Compare(bv);
                }
                break;
            }
            case KEY_EXPIRATION_ELEMENT:
            {
                uint64 aexpire, bexpire;
                found_a = BufferHelper::ReadVarUInt64(ak_buf, aexpire);
                found_b = BufferHelper::ReadVarUInt64(bk_buf, bexpire);
                COMPARE_EXIST(found_a, found_b);
                ret = COMPARE_NUMBER(aexpire, bexpire);
                if (ret == 0)
                {
                    ret = akey.compare(bkey);
                }
                break;
            }
            case BITSET_ELEMENT:
            {
                uint64 aindex, bindex;
                found_a = BufferHelper::ReadVarUInt64(ak_buf, aindex);
                found_b = BufferHelper::ReadVarUInt64(bk_buf, bindex);
                COMPARE_EXIST(found_a, found_b);
                ret = COMPARE_NUMBER(aindex, bindex);
                break;
            }
            case SET_META:
            case ZSET_META:
            case LIST_META:
            case BITSET_META:
            case SCRIPT:
            default:
            {
                break;
            }
        }
        return ret;
    }

    void encode_key(Buffer& buf, const KeyObject& key)
    {
        uint32 header = (uint32) (key.db << 8) + key.type;
        BufferHelper::WriteFixUInt32(buf, header);
        BufferHelper::WriteVarSlice(buf, key.key);
        switch (key.type)
        {
            case HASH_FIELD:
            {
                const HashKeyObject& hk = (const HashKeyObject&) key;
                encode_value(buf, hk.field);
                break;
            }
            case LIST_ELEMENT:
            {
                const ListKeyObject& lk = (const ListKeyObject&) key;
                encode_value(buf, lk.score);
                break;
            }
            case SET_ELEMENT:
            {
                const SetKeyObject& sk = (const SetKeyObject&) key;
                encode_value(buf, sk.value);
                break;
            }
            case ZSET_ELEMENT:
            {
                const ZSetKeyObject& sk = (const ZSetKeyObject&) key;
                encode_value(buf, sk.score);
                encode_value(buf, sk.value);
                break;
            }
            case ZSET_ELEMENT_NODE:
            {
                const ZSetNodeKeyObject& zk = (const ZSetNodeKeyObject&) key;
                encode_value(buf, zk.value);
                break;
            }
            case BITSET_ELEMENT:
            {
                const BitSetKeyObject& bk = (const BitSetKeyObject&) key;
                BufferHelper::WriteVarUInt64(buf, bk.index);
                break;
            }
            case KEY_EXPIRATION_ELEMENT:
            {
                const ExpireKeyObject& bk = (const ExpireKeyObject&) key;
                BufferHelper::WriteVarUInt64(buf, bk.expireat);
                break;
            }
            case LIST_META:
            case ZSET_META:
            case SET_META:
            case BITSET_META:
            case SCRIPT:
            case KEY_META:
            default:
            {
                break;
            }
        }
    }

    bool peek_dbkey_header(const Slice& key, DBID& db, KeyType& type)
    {
        Buffer buf(const_cast<char*>(key.data()), 0, key.size());
        uint32 header;
        if (!BufferHelper::ReadFixUInt32(buf, header))
        {
            return false;
        }
        type = (KeyType) (header & 0xFF);
        db = header >> 8;
        return true;
    }

    KeyObject* decode_key(const Slice& key, KeyObject* expected)
    {
        Buffer buf(const_cast<char*>(key.data()), 0, key.size());
        uint32 header;
        if (!BufferHelper::ReadFixUInt32(buf, header))
        {
            return NULL;
        }
        uint8 type = header & 0xFF;
        uint32 db = header >> 8;
        if (NULL != expected)
        {
            if (type != expected->type || db != expected->db)
            {
                return NULL;
            }
        }
        Slice keystr;
        if (!BufferHelper::ReadVarSlice(buf, keystr))
        {
            return NULL;
        }
        if (NULL != expected)
        {
            if (keystr != expected->key)
            {
                return NULL;
            }
        }
        switch (type)
        {
            case HASH_FIELD:
            {
                HashKeyObject* hk = new HashKeyObject(keystr, Slice(), db);
                if (!decode_value(buf, hk->field))
                {
                    DELETE(hk);
                    return NULL;
                }
                return hk;
            }
            case LIST_ELEMENT:
            {
                ListKeyObject* hk = new ListKeyObject(keystr, ValueData((int64) 0), db);
                if (!decode_value(buf, hk->score))
                {
                    DELETE(hk);
                    return NULL;
                }
                return hk;
            }

            case SET_ELEMENT:
            {
                SetKeyObject* sk = new SetKeyObject(keystr, Slice(), db);
                if (!decode_value(buf, sk->value))
                {
                    DELETE(sk);
                    return NULL;
                }
                return sk;
            }
            case ZSET_ELEMENT:
            {
                ZSetKeyObject* zsk = new ZSetKeyObject(keystr, Slice(), ValueData((int64) 0), db);
                if (!decode_value(buf, zsk->score) || !decode_value(buf, zsk->value))
                {
                    DELETE(zsk);
                    return NULL;
                }
                return zsk;
            }
            case ZSET_ELEMENT_NODE:
            {
                ZSetNodeKeyObject* zsk = new ZSetNodeKeyObject(keystr, Slice(), db);
                if (!decode_value(buf, zsk->value))
                {
                    DELETE(zsk);
                    return NULL;
                }
                return zsk;
            }
            case BITSET_ELEMENT:
            {
                uint64 index;
                if (!BufferHelper::ReadVarUInt64(buf, index))
                {
                    return NULL;
                }
                return new BitSetKeyObject(keystr, index, db);
            }
            case KEY_EXPIRATION_ELEMENT:
            {
                uint64 ts;
                if (!BufferHelper::ReadVarUInt64(buf, ts))
                {
                    return NULL;
                }
                return new ExpireKeyObject(keystr, ts, db);
            }
            case SET_META:
            case ZSET_META:
            case LIST_META:
            case BITSET_META:
            case KEY_META:
            case SCRIPT:
            default:
            {
                return new KeyObject(keystr, (KeyType) type, db);
            }
        }
    }

    void encode_meta(Buffer& buf, CommonMetaValue& meta)
    {
        BufferHelper::WriteFixUInt8(buf, meta.header.version);
        BufferHelper::WriteFixUInt8(buf, meta.header.type);
        BufferHelper::WriteVarUInt64(buf, meta.header.expireat);
        switch (meta.header.type)
        {
            case STRING_META:
            {
                StringMetaValue& bmeta = (StringMetaValue&) meta;
                bmeta.Encode(buf);
                break;
            }
            case BITSET_META:
            {
                BitSetMetaValue& bmeta = (BitSetMetaValue&) meta;
                bmeta.Encode(buf);
                break;
            }
            case HASH_META:
            {
                HashMetaValue & bmeta = (HashMetaValue&) meta;
                bmeta.Encode(buf);
                break;
            }
            case LIST_META:
            {
                ListMetaValue & bmeta = (ListMetaValue&) meta;
                bmeta.Encode(buf);
                break;
            }
            case ZSET_META:
            {
                ZSetMetaValue & bmeta = (ZSetMetaValue&) meta;
                bmeta.Encode(buf);
                break;
            }
            case SET_META:
            {
                SetMetaValue & bmeta = (SetMetaValue&) meta;
                if (bmeta.ziped || bmeta.dirty)
                {
                    bmeta.max.Clear();
                    bmeta.min.Clear();
                }
                bmeta.Encode(buf);
                break;
            }
            default:
            {
                ERROR_LOG("Unsupported meta type:%d", meta.header.type);
                break;
            }
        }
    }

    CommonMetaValue* decode_meta(const char* data, size_t size, bool only_head)
    {
        MetaValueHeader header;
        Buffer msgbuf(const_cast<char*>(data), 0, size);
        uint8 type;
        if (BufferHelper::ReadFixUInt8(msgbuf, header.version) && BufferHelper::ReadFixUInt8(msgbuf, type)
                && BufferHelper::ReadVarUInt64(msgbuf, header.expireat))
        {
            header.type = (KeyType) type;
            if (only_head)
            {
                CommonMetaValue* meta = NULL;
                NEW(meta, CommonMetaValue);
                meta->header = header;
                return meta;
            }
        }
        else
        {
            ERROR_LOG("Decode meta header failed.");
            return NULL;
        }
        CommonMetaValue* meta = NULL;
        switch (header.type)
        {
            case STRING_META:
            {
                StringMetaValue * bmeta = new StringMetaValue;
                if (!bmeta->Decode(msgbuf))
                {
                    ERROR_LOG("Decode string  meta header failed.");
                    DELETE(bmeta);
                }
                meta = bmeta;
                break;
            }
            case BITSET_META:
            {
                BitSetMetaValue* bmeta = new BitSetMetaValue;
                if (!bmeta->Decode(msgbuf))
                {
                    ERROR_LOG("Decode bitset meta failed.");
                    DELETE(bmeta);
                }
                meta = bmeta;
                break;
            }
            case HASH_META:
            {
                HashMetaValue* bmeta = new HashMetaValue;
                if (!bmeta->Decode(msgbuf))
                {
                    ERROR_LOG("Decode hash meta  failed.");
                    DELETE(bmeta);
                }
                meta = bmeta;
                break;
            }
            case LIST_META:
            {
                ListMetaValue* bmeta = new ListMetaValue;
                if (!bmeta->Decode(msgbuf))
                {
                    ERROR_LOG("Decode list meta  failed.");
                    DELETE(bmeta);
                }
                meta = bmeta;
                break;
            }
            case ZSET_META:
            {
                ZSetMetaValue* bmeta = new ZSetMetaValue;
                if (!bmeta->Decode(msgbuf))
                {
                    ERROR_LOG("Decode zset meta  failed.");
                    DELETE(bmeta);
                }
                meta = bmeta;
                break;
            }
            case SET_META:
            {
                SetMetaValue* smeta = new SetMetaValue;
                if (!smeta->Decode(msgbuf))
                {
                    ERROR_LOG("Decode set meta  failed.");
                    DELETE(smeta);
                }
                meta = smeta;
                break;
            }
            default:
            {
                ERROR_LOG("Unsupported meta type:%d", header.type);
                return NULL;
            }
        }
        if (NULL != meta)
        {
            meta->header = header;
        }
        return meta;
    }

    ValueObject* decode_value_obj(KeyType type, const char* data, size_t size)
    {
        Buffer buffer(const_cast<char*>(data), 0, size);
        switch (type)
        {
            case KEY_META:
            {
                return decode_meta(data, size, false);
            }
            case ZSET_ELEMENT_NODE:
            {
                ZSetNodeValueObject* obj = new ZSetNodeValueObject;
                obj->Decode(buffer);
                return obj;
                break;
            }
            case HASH_FIELD:
            case LIST_ELEMENT:
            case ZSET_ELEMENT:
            case SCRIPT:
            {
                CommonValueObject* obj = new CommonValueObject;
                obj->Decode(buffer);
                return obj;
            }
            case BITSET_ELEMENT:
            {
                BitSetElementValue* obj = new BitSetElementValue;
                obj->Decode(buffer);
                return obj;
            }
            case KEY_EXPIRATION_ELEMENT:
            case SET_ELEMENT:
            default:
            {
                return new EmptyValueObject;
            }
        }
    }

    void encode_value(Buffer& buf, const ValueData& value)
    {
        value.Encode(buf);
    }

    bool decode_value(Buffer& buf, ValueData& value, bool to_slice)
    {
        return value.Decode(buf, to_slice);
    }

    bool decode_value_by_string(const std::string& str, ValueData& value)
    {
        Buffer buf(const_cast<char*>(str.data()), 0, str.size());
        return decode_value(buf, value);
    }

    void next_key(const Slice& key, std::string& next)
    {
        next.assign(key.data(), key.size());
        for (uint32 i = next.size(); i > 0; i--)
        {
            if (next[i - 1] < 0x7F)
            {
                next[i - 1] = next[i - 1] + 1;
                return;
            }
        }
        next.append("\0", 1);
    }

    SetKeyObject::SetKeyObject(const Slice& k, const ValueData& v, DBID id) :
            KeyObject(k, SET_ELEMENT, id), value(v)
    {

    }
    SetKeyObject::SetKeyObject(const Slice& k, const Slice& v, DBID id) :
            KeyObject(k, SET_ELEMENT, id)
    {
        value.SetValue(v, true);
    }

    int GeoAddOptions::Parse(const StringArray& args, std::string& err, uint32 off)
    {
        if (!strcasecmp(args[off].c_str(), "wgs84"))
        {
            coord_type = GEO_WGS84_TYPE;
        }
        else if (!strcasecmp(args[off].c_str(), "mercator"))
        {
            coord_type = GEO_MERCATOR_TYPE;
        }
        else
        {
            WARN_LOG("Invalid coord-type:%s.", args[off].c_str());
            return -1;
        }
        off++;
        if (!string_todouble(args[off], x) || !string_todouble(args[off + 1], y))
        {
            err = "Invalid coodinates " + args[off] + "/" + args[off + 1];
            return -1;
        }
        if (!GeoHashHelper::VerifyCoordinates(coord_type, x, y))
        {
            err = "Invalid coodinates " + args[off] + "/" + args[off + 1];
            return -1;
        }
        off += 2;
        value = args[off++];
        for (; off < args.size(); off += 2)
        {
            if (off + 1 >= args.size())
            {
                err = "Invalid attribute " + args[off] + " with no value followed.";
                return -1;
            }
            attrs[args[off]] = args[off + 1];
        }
        return 0;
    }

    int GeoSearchOptions::Parse(const StringArray& args, std::string& err, uint32 off)
    {
        for (uint32 i = off; i < args.size(); i++)
        {
            if (!strcasecmp(args[i].c_str(), "asc"))
            {
                nosort = false;
                asc = true;
            }
            else if (!strcasecmp(args[i].c_str(), "desc"))
            {
                asc = false;
                nosort = false;
            }
            else if (!strcasecmp(args[i].c_str(), "limit") && i < args.size() - 2)
            {
                if (!string_toint32(args[i + 1], offset) || !string_toint32(args[i + 2], limit))
                {
                    err = "Invalid limit/offset value.";
                    return -1;
                }
                i += 2;
            }
            else if (!strcasecmp(args[i].c_str(), "RADIUS") && i < args.size() - 1)
            {
                if (!string_touint32(args[i + 1], radius) || radius < 1 || radius >= 10000000)
                {
                    err = "Invalid radius value.";
                    return -1;
                }
                i++;
            }
            else if ((!strcasecmp(args[i].c_str(), "mercator") || !strcasecmp(args[i].c_str(), "wgs84"))
                    && i < args.size() - 2)
            {
                if (!string_todouble(args[i + 1], x) || !string_todouble(args[i + 2], y))
                {
                    err = "Invalid location value.";
                    return -1;
                }
                if (!strcasecmp(args[i].c_str(), "wgs84"))
                {
                    coord_type = GEO_WGS84_TYPE;
                }
                else if (!strcasecmp(args[i].c_str(), "mercator"))
                {
                    coord_type = GEO_MERCATOR_TYPE;
                }
                if (!GeoHashHelper::VerifyCoordinates(coord_type, x, y))
                {
                    err = "Invalid location value.";
                    return -1;
                }
                by_location = true;
                i += 2;
            }
            else if (!strcasecmp(args[i].c_str(), "member") && i < args.size() - 1)
            {
                member = args[i + 1];
                by_member = true;
                i++;
            }
            else if (!strcasecmp(args[i].c_str(), "in") && i < args.size() - 1)
            {
                uint32 len;
                if (!string_touint32(args[i + 1], len) || (i + 1 + len) > args.size() - 1)
                {
                    err = "Invalid member value.";
                    return -1;
                }
                for (uint32 j = 0; j < len; j++)
                {
                    submembers.insert(args[i + 2 + j]);
                }
                in_members = true;
                i = i + len + 1;
            }
            else if ((!strcasecmp(args[i].c_str(), "GET")) && i < args.size() - 1)
            {
                GeoSearchGetOption get;
                get.get_pattern = args[i + 1];
                if (get.get_pattern.size() > 2 && !strncasecmp(get.get_pattern.data(), "#.", 2))
                {
                    get.get_attr = true;
                    get.get_pattern = args[i + 1].substr(2);
                }
                get_patterns.push_back(get);
                i++;
            }
            else if ((!strcasecmp(args[i].c_str(), "include")) && i < args.size() - 2)
            {
                if (!includes.insert(StringStringMap::value_type(args[i + 1], args[i + 2])).second)
                {
                    err = "duplicate include key pattern:" + args[i + 1];
                    return -1;
                }
                i += 2;
            }
            else if ((!strcasecmp(args[i].c_str(), "exclude")) && i < args.size() - 2)
            {
                if (!excludes.insert(StringStringMap::value_type(args[i + 1], args[i + 2])).second)
                {
                    err = "duplicate exclude key pattern:" + args[i + 1];
                    return -1;
                }
                i += 2;
            }
            else if ((!strcasecmp(args[i].c_str(), GEO_SEARCH_WITH_COORDINATES)))
            {
                GeoSearchGetOption get;
                get.get_coodinates = true;
                get_patterns.push_back(get);
            }
            else if ((!strcasecmp(args[i].c_str(), GEO_SEARCH_WITH_DISTANCES)))
            {
                GeoSearchGetOption get;
                get.get_distances = true;
                get_patterns.push_back(get);
            }
            else
            {
                DEBUG_LOG("Invalid geosearch option:%s", args[i].c_str());
                err = "Invalid geosearch options.";
                return -1;
            }
        }

        if (radius < 1)
        {
            err = "no radius specified";
            return -1;
        }
        if (!by_location && !by_member)
        {
            err = "no location/member specified";
            return -1;
        }
        return 0;
    }

    int AreaAddOptions::Parse(const StringArray& args, std::string& err, uint32 off)
    {
        value = args[off++];
        if (!strcasecmp(args[off].c_str(), "wgs84"))
        {
            coord_type = GEO_WGS84_TYPE;
        }
        else if (!strcasecmp(args[off].c_str(), "mercator"))
        {
            coord_type = GEO_MERCATOR_TYPE;
        }
        else
        {
            WARN_LOG("Invalid coord-type:%s.", args[off].c_str());
            return -1;
        }
        off++;
        while (off + 1 < args.size())
        {
            double x, y;
            if (!string_todouble(args[off], x) || !string_todouble(args[off + 1], y))
            {
                err = "Invalid coodinates " + args[off] + "/" + args[off + 1];
                return -1;
            }
            if (!GeoHashHelper::VerifyCoordinates(coord_type, x, y))
            {
                err = "Invalid coodinates " + args[off] + "/" + args[off + 1];
                return -1;
            }
            xx.push_back(x);
            yy.push_back(y);
            off += 2;
        }
        if (xx.size() < 3)
        {
            err = "At least 3 vertex needed.";
            return -1;
        }
        if (off < args.size())
        {
            err = "Rest arg:" + args[off];
            return -1;
        }
        return 0;
    }

    int AreaLocateOptions::Parse(const StringArray& args, std::string& err, uint32 off)
    {
        if (!strcasecmp(args[off].c_str(), "wgs84"))
        {
            coord_type = GEO_WGS84_TYPE;
        }
        else if (!strcasecmp(args[off].c_str(), "mercator"))
        {
            coord_type = GEO_MERCATOR_TYPE;
        }
        else
        {
            WARN_LOG("Invalid coord-type:%s.", args[off].c_str());
            return -1;
        }
        off++;
        if (!string_todouble(args[off], x) || !string_todouble(args[off + 1], y))
        {
            err = "Invalid coodinates " + args[off] + "/" + args[off + 1];
            return -1;
        }
        if (!GeoHashHelper::VerifyCoordinates(coord_type, x, y))
        {
            err = "Invalid coodinates " + args[off] + "/" + args[off + 1];
            return -1;
        }
        off += 2;
        for (; off < args.size(); off++)
        {
            get_patterns.push_back(args[off]);
        }
        return 0;
    }

    bool ZSetQueryOptions::Parse(const StringArray& cmd, uint32 idx)
    {
        for (uint32 i = idx; i < cmd.size(); i++)
        {
            if (!strcasecmp(cmd[i].c_str(), "withscores"))
            {
                withscores = true;
            }
            else if (!strcasecmp(cmd[i].c_str(), "limit"))
            {
                if (i + 2 >= cmd.size())
                {
                    return false;
                }
                if (!string_toint32(cmd[i + 1], limit_offset) || !string_toint32(cmd[i + 2], limit_count))
                {
                    return false;
                }
                withlimit = true;
                i += 2;
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    static bool verify_lexrange_para(const std::string& str)
    {
        if (str == "-" || str == "+")
        {
            return true;
        }
        if (str.empty())
        {
            return false;
        }

        if (str[0] != '(' && str[0] != '[')
        {
            return false;
        }
        return true;
    }

    bool LexRange::Parse(const std::string& minstr, const std::string& maxstr)
    {
        if (!verify_lexrange_para(minstr) || !verify_lexrange_para(maxstr))
        {
            return false;
        }
        if (minstr[0] == '(')
        {
            include_min = false;
        }
        if (minstr[0] == '[')
        {
            include_min = true;
        }
        if (maxstr[0] == '(')
        {
            include_max = false;
        }
        if (maxstr[0] == '[')
        {
            include_max = true;
        }

        if (minstr == "-")
        {
            min.clear();
            include_min = true;
        }
        else
        {
            min = minstr.substr(1);
        }
        if (maxstr == "+")
        {
            max.clear();
            include_max = true;
        }
        else
        {
            max = maxstr.substr(1);
        }
        if (min > max && !max.empty())
        {
            return false;
        }
        return true;
    }
}

