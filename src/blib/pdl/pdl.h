#pragma once

#include <blib/blibint.h>
#include <blib/utilmacro.h>

#include <string>
#include <vector>

namespace blib
{
	namespace pdl
	{
		class Parser
		{
		private:
			struct Data;
			struct Loader;
			Data* d;

		public:
			Parser();
			~Parser();

			Parser(const Parser&) = delete;
			Parser& operator=(const Parser&) = delete;

			bool load(_In const std::string& path);
			bool parse(_In const std::string& text);

			bool parseNext(_In const std::string& text, _In _Out size_t& offset);;

			template<class T>
			bool getVar(_In const std::string& name, _Out T& out) const
			{
				return getVarImpl(*d, name, out);
			}

			const std::string& getLastError() const;

		private:
			bool getVarImpl(const Data& data, const std::string& name, std::string& out) const;
			bool getVarImpl(const Data& data, const std::string& name, bint32& out) const;
			bool getVarImpl(const Data& data, const std::string& name, float& out) const;
			bool getVarImpl(const Data& data, const std::string& name, bool& out) const;
			bool getVarImpl(const Data& data, const std::string& name, std::vector<std::string>& out) const;
			bool getVarImpl(const Data& data, const std::string& name, std::vector<bint32>& out) const;
			bool getVarImpl(const Data& data, const std::string& name, std::vector<float>& out) const;
			bool getVarImpl(const Data& data, const std::string& name, std::vector<bool>& out) const;
		};
	}
}
