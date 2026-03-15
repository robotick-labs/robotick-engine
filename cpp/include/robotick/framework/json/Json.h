// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringView.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace robotick::json
{
	class Value;

	class StringSink
	{
	  public:
		using Ch = char;

		void Put(char c) { buffer.Put(c); }
		void Flush() {}
		char Peek() const { return '\0'; }
		char Take() { return '\0'; }
		size_t Tell() const { return buffer.GetSize(); }
		char* PutBegin() { return nullptr; }
		size_t PutEnd(char*) { return 0; }

		const char* c_str() const { return buffer.GetString(); }
		size_t size() const { return buffer.GetSize(); }

	  private:
		rapidjson::StringBuffer buffer;
	};

	template <typename FlushFn, size_t BufferSize = 512> class ChunkedSink
	{
	  public:
		using Ch = char;

		explicit ChunkedSink(FlushFn flush_fn)
			: flush_fn(flush_fn)
		{
		}

		~ChunkedSink() { Flush(); }

		void Put(char c)
		{
			if (size == BufferSize)
			{
				Flush();
			}
			buffer[size++] = c;
			++written;
		}

		void Flush()
		{
			if (size == 0)
			{
				return;
			}
			flush_fn(buffer, size);
			size = 0;
		}

		char Peek() const { return '\0'; }
		char Take() { return '\0'; }
		size_t Tell() const { return written; }
		char* PutBegin() { return nullptr; }
		size_t PutEnd(char*) { return 0; }

	  private:
		FlushFn flush_fn;
		char buffer[BufferSize] = {};
		size_t size = 0;
		size_t written = 0;
	};

	template <typename Sink> class Writer
	{
	  public:
		explicit Writer(Sink& sink_in)
			: sink(sink_in)
			, writer(sink_in)
		{
		}

		bool start_object() { return writer.StartObject(); }
		bool end_object() { return writer.EndObject(); }
		bool start_array() { return writer.StartArray(); }
		bool end_array() { return writer.EndArray(); }
		bool key(const char* key_text) { return writer.Key(key_text ? key_text : ""); }
		bool string(const char* value) { return writer.String(value ? value : ""); }
		template <size_t N> bool string(const FixedString<N>& value) { return writer.String(value.c_str(), value.length()); }
		bool boolean(bool value) { return writer.Bool(value); }
		bool int32(int value) { return writer.Int(value); }
		bool int64(int64_t value) { return writer.Int64(value); }
		bool uint32(unsigned value) { return writer.Uint(value); }
		bool uint64(uint64_t value) { return writer.Uint64(value); }
		bool real(double value) { return writer.Double(value); }
		bool null() { return writer.Null(); }
		void flush() { sink.Flush(); }

	  private:
		template <size_t N> friend bool compact_to_fixed_string(const Value& value, FixedString<N>& out);
		friend class Value;

		Sink& sink;
		rapidjson::Writer<Sink> writer;
	};

	class Value
	{
	  public:
		Value() = default;

		bool is_valid() const { return value != nullptr; }
		bool is_null() const { return value && value->IsNull(); }
		bool is_object() const { return value && value->IsObject(); }
		bool is_array() const { return value && value->IsArray(); }
		bool is_string() const { return value && value->IsString(); }
		bool is_bool() const { return value && value->IsBool(); }
		bool is_number() const { return value && value->IsNumber(); }
		bool is_integer() const { return value && value->IsInt64(); }

		bool contains(const char* key) const { return is_object() && key && value->HasMember(key); }

		Value operator[](const char* key) const
		{
			if (!is_object() || !key)
			{
				return {};
			}
			auto it = value->FindMember(key);
			return it == value->MemberEnd() ? Value() : Value(&it->value);
		}

		Value operator[](size_t index) const
		{
			if (!is_array() || index >= value->Size())
			{
				return {};
			}
			return Value(&((*value)[static_cast<rapidjson::SizeType>(index)]));
		}

		size_t size() const
		{
			if (is_array())
			{
				return value->Size();
			}
			if (is_object())
			{
				return value->MemberCount();
			}
			return 0;
		}

		const char* get_c_string(const char* fallback = "") const { return is_string() ? value->GetString() : fallback; }

		bool get_bool(const bool fallback = false) const { return is_bool() ? value->GetBool() : fallback; }

		int64_t get_int64(const int64_t fallback = 0) const
		{
			if (!value)
			{
				return fallback;
			}
			if (value->IsInt64())
			{
				return value->GetInt64();
			}
			if (value->IsInt())
			{
				return value->GetInt();
			}
			if (value->IsUint())
			{
				return static_cast<int64_t>(value->GetUint());
			}
			if (value->IsUint64())
			{
				return static_cast<int64_t>(value->GetUint64());
			}
			return fallback;
		}

		uint64_t get_uint64(const uint64_t fallback = 0) const
		{
			if (!value)
			{
				return fallback;
			}
			if (value->IsUint64())
			{
				return value->GetUint64();
			}
			if (value->IsUint())
			{
				return value->GetUint();
			}
			if (value->IsInt64())
			{
				const int64_t signed_value = value->GetInt64();
				return signed_value >= 0 ? static_cast<uint64_t>(signed_value) : fallback;
			}
			return fallback;
		}

		double get_double(const double fallback = 0.0) const { return value && value->IsNumber() ? value->GetDouble() : fallback; }

		bool equals(const char* text) const { return is_string() && text && StringView(value->GetString()).equals(text); }

		template <typename Fn> void for_each_array(Fn&& fn) const
		{
			if (!is_array())
			{
				return;
			}
			for (auto it = value->Begin(); it != value->End(); ++it)
			{
				fn(Value(&(*it)));
			}
		}

	  private:
		template <size_t N> friend bool compact_to_fixed_string(const Value& value, FixedString<N>& out);
		friend class Document;

		explicit Value(const rapidjson::Value* value_in)
			: value(value_in)
		{
		}

		const rapidjson::Value* value = nullptr;
	};

	class Document
	{
	  public:
		bool parse(const char* text)
		{
			document.Parse(text ? text : "");
			return !document.HasParseError();
		}

		bool parse(const void* data, const size_t size)
		{
			if (!data)
			{
				document.Parse("");
			}
			else
			{
				document.Parse(reinterpret_cast<const char*>(data), size);
			}
			return !document.HasParseError();
		}

		bool parse_in_situ(char* text)
		{
			document.ParseInsitu(text ? text : empty_buffer);
			return !document.HasParseError();
		}

		Value root() const { return document.HasParseError() ? Value() : Value(&document); }

		size_t get_error_offset() const { return document.HasParseError() ? document.GetErrorOffset() : 0; }

	  private:
		char empty_buffer[1] = {'\0'};
		rapidjson::Document document;
	};

	template <size_t N> bool assign_sink_to_fixed_string(const StringSink& sink, FixedString<N>& out)
	{
		if (sink.size() >= N)
		{
			return false;
		}
		out.assign(sink.c_str(), sink.size());
		return true;
	}

	template <size_t N> bool compact_to_fixed_string(const Value& value, FixedString<N>& out)
	{
		if (!value.is_valid())
		{
			return false;
		}

		StringSink sink;
		Writer<StringSink> writer(sink);
		if (!value.value->Accept(writer.writer))
		{
			return false;
		}
		return assign_sink_to_fixed_string(sink, out);
	}

	template <size_t N> bool normalize_to_fixed_string(const char* text, FixedString<N>& out)
	{
		Document document;
		return document.parse(text) && compact_to_fixed_string(document.root(), out);
	}

	template <size_t N> bool scalar_to_fixed_string(const Value& value, FixedString<N>& out)
	{
		if (!value.is_valid())
		{
			return false;
		}
		if (value.is_string())
		{
			out = value.get_c_string();
			return true;
		}
		if (value.is_bool())
		{
			out = value.get_bool() ? "true" : "false";
			return true;
		}
		if (value.is_number())
		{
			return compact_to_fixed_string(value, out);
		}
		return false;
	}
} // namespace robotick::json
