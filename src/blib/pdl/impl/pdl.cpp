#include <blib/pdl/pdl.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace blib
{
	namespace pdl
	{
		// ============================================================
		// value types
		// ============================================================

		enum class ValueType : buint8
		{
			None,
			String,
			Int,
			Float,
			Bool,
			AnyList
		};

		struct Value
		{
			ValueType type;
			std::string str;
			bint32 i;
			float f;
			bool b;
			std::vector<Value> list;

			Value()
				: type(ValueType::None)
				, i(0)
				, f(0.0f)
				, b(false)
			{
			}

			void clear()
			{
				type = ValueType::None;
				str.clear();
				i = 0;
				f = 0.0f;
				b = false;
				list.clear();
			}
		};

		enum class VarType : buint8
		{
			String,
			Int,
			Float,
			Bool,
			ArrayString,
			ArrayInt,
			ArrayFloat,
			ArrayBool
		};

		struct Var
		{
			std::string name;
			VarType type;
			Value value;
			bool captured;
		};

		// ============================================================
		// grammar structures
		// ============================================================

		struct Node;

		enum class ElementKind : buint8
		{
			Field,
			Optional,
			Terminator,
			Everything,
			Any,
			Empty
		};

		struct Element
		{
			ElementKind kind;
			Node* node;
			std::vector<Element> inner;

			Element()
				: kind(ElementKind::Terminator)
				, node(nullptr)
			{
			}
		};

		enum class AltKind : buint8
		{
			Sequence,
			LiteralSet
		};

		struct Literal
		{
			ValueType type;
			std::string text;
		};

		struct Guard
		{
			Node* node;
			std::string text;
		};

		struct Alternative
		{
			AltKind kind;
			std::vector<Element> elems;
			std::vector<Literal> literals;
			std::vector<Guard> guards;

			Alternative()
				: kind(AltKind::LiteralSet)
			{
			}
		};

		struct Node
		{
			std::string name;
			Node* parent;
			std::vector<Alternative> alts;

			Node()
				: parent(nullptr)
			{
			}
		};

		struct Binding
		{
			Var* var;
			Node* node;
		};

		// ============================================================
		// Parser::Data
		// ============================================================

		struct Parser::Data
		{
			std::string error;

			std::map<std::string, Var> vars;
			std::map<std::string, Node> nodes;
			std::vector<Node*> roots;
			std::vector<Binding> bindings;
			std::vector<std::string> terminatorLiterals;

			Node* entry;

			Data()
				: entry(nullptr)
			{
			}

			void clearGrammar()
			{
				error.clear();
				vars.clear();
				nodes.clear();
				roots.clear();
				bindings.clear();
				terminatorLiterals.clear();
				entry = nullptr;
			}

			void resetVars()
			{
				for (std::map<std::string, Var>::iterator it = vars.begin(); it != vars.end(); ++it)
				{
					it->second.value.clear();
					it->second.captured = false;
				}
			}
		};

		template<class T>
		static std::string toStr(T v)
		{
			return std::to_string(v);
		}

		// ============================================================
		// pdl lexer
		// ============================================================

		enum class TokKind : buint8
		{
			End,
			DoubleColon,
			Colon,
			Semicolon,
			Comma,
			Assign,
			PlusAssign,
			LAngle,
			RAngle,
			LBracket,
			RBracket,
			LBrace,
			RBrace,
			At,
			EqEq,
			Ident,
			String,
			Number
		};

		struct Token
		{
			TokKind kind;
			std::string text;
			buint32 line;
		};

		static bool lex(const std::string& src, std::vector<Token>& out, std::string& error)
		{
			const buint32 n = (buint32)src.size();
			buint32 i = 0;
			buint32 line = 1;

			while (i < n)
			{
				char c = src[i];

				if (c == ' ' || c == '\t' || c == '\r')
				{
					++i;
					continue;
				}
				if (c == '\n')
				{
					++i;
					++line;
					continue;
				}
				if (c == '/' && i + 1 < n && src[i + 1] == '/')
				{
					while (i < n && src[i] != '\n')
						++i;
					continue;
				}
				if (c == '/' && i + 1 < n && src[i + 1] == '*')
				{
					const buint32 startLine = line;
					i += 2;
					bool closed = false;
					while (i < n)
					{
						if (src[i] == '\n')
							++line;
						if (src[i] == '*' && i + 1 < n && src[i + 1] == '/')
						{
							i += 2;
							closed = true;
							break;
						}
						++i;
					}
					if (!closed)
					{
						error = "unterminated comment at line " + toStr(startLine);
						return false;
					}
					continue;
				}

				Token t;
				t.line = line;

				if (c == ':')
				{
					if (i + 1 < n && src[i + 1] == ':')
					{
						t.kind = TokKind::DoubleColon;
						i += 2;
					}
					else
					{
						t.kind = TokKind::Colon;
						++i;
					}
				}
				else if (c == ';')
				{
					t.kind = TokKind::Semicolon;
					++i;
				}
				else if (c == ',')
				{
					t.kind = TokKind::Comma;
					++i;
				}
				else if (c == '=')
				{
					if (i + 1 < n && src[i + 1] == '=')
					{
						t.kind = TokKind::EqEq;
						i += 2;
					}
					else
					{
						t.kind = TokKind::Assign;
						++i;
					}
				}
				else if (c == '+')
				{
					if (i + 1 < n && src[i + 1] == '=')
					{
						t.kind = TokKind::PlusAssign;
						i += 2;
					}
					else
					{
						error = "unexpected character '+' at line " + toStr(line);
						return false;
					}
				}
				else if (c == '<')
				{
					t.kind = TokKind::LAngle;
					++i;
				}
				else if (c == '>')
				{
					t.kind = TokKind::RAngle;
					++i;
				}
				else if (c == '[')
				{
					t.kind = TokKind::LBracket;
					++i;
				}
				else if (c == ']')
				{
					t.kind = TokKind::RBracket;
					++i;
				}
				else if (c == '{')
				{
					t.kind = TokKind::LBrace;
					++i;
				}
				else if (c == '}')
				{
					t.kind = TokKind::RBrace;
					++i;
				}
				else if (c == '@')
				{
					t.kind = TokKind::At;
					++i;
				}
				else if (c == '"')
				{
					t.kind = TokKind::String;
					++i;
					bool closed = false;
					while (i < n)
					{
						char ch = src[i];
						if (ch == '\\' && i + 1 < n && (src[i + 1] == '"' || src[i + 1] == '\\'))
						{
							t.text += src[i + 1];
							i += 2;
							continue;
						}
						if (ch == '"')
						{
							++i;
							closed = true;
							break;
						}
						if (ch == '\n')
							++line;
						t.text += ch;
						++i;
					}
					if (!closed)
					{
						error = "unterminated string at line " + toStr(line);
						return false;
					}
				}
				else if (std::isdigit((unsigned char)c) ||
					(c == '-' && i + 1 < n && std::isdigit((unsigned char)src[i + 1])))
				{
					t.kind = TokKind::Number;
					if (c == '-')
					{
						t.text += c;
						++i;
					}
					while (i < n && std::isdigit((unsigned char)src[i]))
					{
						t.text += src[i];
						++i;
					}
					if (i < n && src[i] == '.')
					{
						t.text += src[i];
						++i;
						while (i < n && std::isdigit((unsigned char)src[i]))
						{
							t.text += src[i];
							++i;
						}
					}
				}
				else if (std::isalpha((unsigned char)c) || c == '_')
				{
					t.kind = TokKind::Ident;
					while (i < n && (std::isalnum((unsigned char)src[i]) || src[i] == '_'))
					{
						t.text += src[i];
						++i;
					}
				}
				else
				{
					error = "unexpected character '" + std::string(1, c) + "' at line " + toStr(line);
					return false;
				}

				out.push_back(t);
			}

			Token end;
			end.kind = TokKind::End;
			end.line = line;
			out.push_back(end);
			return true;
		}

		// ============================================================
		// pdl loader
		// ============================================================

		struct Parser::Loader
		{
			const std::vector<Token>* toks;
			buint32 i;
			Data* data;
			std::string error;

			Loader()
				: toks(nullptr)
				, i(0)
				, data(nullptr)
			{
			}

			const Token& tok() const
			{
				return (*toks)[i];
			}

			bool at(TokKind k) const
			{
				return tok().kind == k;
			}

			bool take(TokKind k)
			{
				if (at(k))
				{
					++i;
					return true;
				}
				return false;
			}

			std::string takeText(TokKind k)
			{
				if (at(k))
				{
					std::string s = tok().text;
					++i;
					return s;
				}
				return std::string();
			}

			std::string errLine() const
			{
				return " at line " + toStr(tok().line);
			}

			Node* resolveNode(const std::string& path)
			{
				std::map<std::string, Node>::iterator it = data->nodes.find(path);
				if (it != data->nodes.end())
					return &it->second;

				Node n;
				n.name = path;

				std::string::size_type sep = path.rfind("::<");
				if (sep != std::string::npos)
					n.parent = resolveNode(path.substr(0, sep));

				data->nodes[path] = n;
				Node* res = &data->nodes[path];
				if (res->parent == nullptr)
					data->roots.push_back(res);
				return res;
			}

			bool parseProgram()
			{
				while (!at(TokKind::End))
				{
					if (at(TokKind::Ident) && tok().text == "let")
					{
						if (!parseDecl())
							return false;
					}
					else if (at(TokKind::At))
					{
						if (!parseBinding())
							return false;
					}
					else if (at(TokKind::String))
					{
						if (!parseTerminatorDecl())
							return false;
					}
					else
					{
						std::vector<Guard> guards;
						while (at(TokKind::LBracket))
						{
							Guard g;
							if (!parseGuard(g))
								return false;
							guards.push_back(g);
						}
						if (at(TokKind::LBrace))
						{
							if (!parseBlock(guards))
								return false;
						}
						else if (at(TokKind::DoubleColon))
						{
							if (!parseRuleDef(guards))
								return false;
						}
						else
						{
							error = "unexpected token '" + tok().text + "'" + errLine();
							return false;
						}
					}
				}
				return true;
			}

			bool parseDecl()
			{
				// at 'let'
				++i;
				if (!(at(TokKind::Ident) && tok().text == "var"))
				{
					error = "expected 'var'" + errLine();
					return false;
				}
				++i;

				VarType type;
				if (!parseType(type))
					return false;

				if (!take(TokKind::At))
				{
					error = "expected '@'" + errLine();
					return false;
				}
				std::string name = takeText(TokKind::Ident);
				if (name.empty())
				{
					error = "expected variable name" + errLine();
					return false;
				}
				name = "@" + name;

				if (!take(TokKind::Semicolon))
				{
					error = "expected ';'" + errLine();
					return false;
				}

				if (data->vars.find(name) != data->vars.end())
				{
					error = "variable '" + name + "' is already declared" + errLine();
					return false;
				}

				Var v;
				v.name = name;
				v.type = type;
				v.captured = false;
				data->vars[name] = v;
				return true;
			}

			bool parseType(VarType& type)
			{
				if (at(TokKind::Ident) && tok().text == "array")
				{
					++i;
					if (!take(TokKind::LAngle))
					{
						error = "expected '<' after 'array'" + errLine();
						return false;
					}
					VarType inner;
					if (!parseType(inner))
						return false;
					if (!take(TokKind::RAngle))
					{
						error = "expected '>'" + errLine();
						return false;
					}
					switch (inner)
					{
					case VarType::String: type = VarType::ArrayString; break;
					case VarType::Int: type = VarType::ArrayInt; break;
					case VarType::Float: type = VarType::ArrayFloat; break;
					case VarType::Bool: type = VarType::ArrayBool; break;
					default:
						error = "nested arrays are not supported" + errLine();
						return false;
					}
					return true;
				}

				if (!at(TokKind::Ident))
				{
					error = "expected type" + errLine();
					return false;
				}
				const std::string& s = tok().text;
				if (s == "string") type = VarType::String;
				else if (s == "int") type = VarType::Int;
				else if (s == "float") type = VarType::Float;
				else if (s == "bool") type = VarType::Bool;
				else
				{
					error = "unknown type '" + s + "'" + errLine();
					return false;
				}
				++i;
				return true;
			}

			bool parseBinding()
			{
				// at '@'
				const buint32 line = tok().line;
				++i;
				std::string name = takeText(TokKind::Ident);
				if (name.empty())
				{
					error = "expected variable name" + errLine();
					return false;
				}
				name = "@" + name;

				if (!take(TokKind::Assign))
				{
					error = "expected '='" + errLine();
					return false;
				}

				std::string path;
				if (!parseNodeRef(path))
					return false;

				if (!take(TokKind::Semicolon))
				{
					error = "expected ';'" + errLine();
					return false;
				}

				std::map<std::string, Var>::iterator vit = data->vars.find(name);
				if (vit == data->vars.end())
				{
					error = "unknown variable '" + name + "' at line " + toStr(line);
					return false;
				}

				for (std::vector<Binding>::iterator it = data->bindings.begin(); it != data->bindings.end(); ++it)
				{
					if (it->var->name == name)
					{
						error = "variable '" + name + "' is already bound at line " + toStr(line);
						return false;
					}
				}

				Binding b;
				b.var = &vit->second;
				b.node = resolveNode(path);
				data->bindings.push_back(b);
				return true;
			}

			bool parseNodeRef(std::string& path)
			{
				if (!take(TokKind::DoubleColon))
				{
					error = "expected '::'" + errLine();
					return false;
				}
				std::string name = takeText(TokKind::Ident);
				if (name.empty())
				{
					error = "expected rule name" + errLine();
					return false;
				}
				path = name;

				while (take(TokKind::DoubleColon))
				{
					if (!take(TokKind::LAngle))
					{
						error = "expected '<'" + errLine();
						return false;
					}
					std::string field = takeText(TokKind::Ident);
					if (field.empty())
					{
						error = "expected field name" + errLine();
						return false;
					}
					if (!take(TokKind::RAngle))
					{
						error = "expected '>'" + errLine();
						return false;
					}
					path += "::<" + field + ">";
				}
				return true;
			}

			bool parseGuard(Guard& g)
			{
				// at '['
				++i;
				std::string path;
				if (!parseNodeRef(path))
					return false;
				if (!take(TokKind::EqEq))
				{
					error = "expected '=='" + errLine();
					return false;
				}
				Literal lit;
				if (!parseLiteral(lit))
					return false;
				if (!take(TokKind::RBracket))
				{
					error = "expected ']'" + errLine();
					return false;
				}
				g.node = resolveNode(path);
				g.text = lit.text;
				return true;
			}

			bool parseLiteral(Literal& lit)
			{
				const Token& t = tok();
				if (t.kind == TokKind::String)
				{
					lit.type = ValueType::String;
					lit.text = t.text;
					++i;
					return true;
				}
				if (t.kind == TokKind::Number)
				{
					lit.type = (t.text.find('.') != std::string::npos) ? ValueType::Float : ValueType::Int;
					lit.text = t.text;
					++i;
					return true;
				}
				if (t.kind == TokKind::Ident && (t.text == "true" || t.text == "false"))
				{
					lit.type = ValueType::Bool;
					lit.text = t.text;
					++i;
					return true;
				}
				error = "expected literal" + errLine();
				return false;
			}

			bool parseSequence(Node* node, std::vector<Element>& elems, TokKind endKind)
			{
				while (!at(endKind) && !at(TokKind::End))
				{
					Element e;
					if (at(TokKind::LAngle))
					{
						++i;
						std::string field = takeText(TokKind::Ident);
						if (field.empty())
						{
							error = "expected field name in sequence of '" + node->name + "'" + errLine();
							return false;
						}
						if (!take(TokKind::RAngle))
						{
							error = "expected '>' in sequence of '" + node->name + "'" + errLine();
							return false;
						}
						e.kind = ElementKind::Field;
						e.node = resolveNode(node->name + "::<" + field + ">");
					}
					else if (at(TokKind::DoubleColon))
				{
					std::string ref;
					if (!parseNodeRef(ref))
						return false;
					e.kind = ElementKind::Field;
					e.node = resolveNode(ref);
				}
				else if (at(TokKind::LBracket))
					{
						++i;
						e.kind = ElementKind::Optional;
						e.node = nullptr;
						if (!parseSequence(node, e.inner, TokKind::RBracket))
							return false;
						if (!take(TokKind::RBracket))
						{
							error = "expected ']'" + errLine();
							return false;
						}
					}
					else if (at(TokKind::Ident) && tok().text == "__terminator")
					{
						++i;
						e.kind = ElementKind::Terminator;
						e.node = nullptr;
					}
					else if (at(TokKind::Ident) && tok().text == "__everything")
					{
						++i;
						e.kind = ElementKind::Everything;
						e.node = nullptr;
					}
					else if (at(TokKind::Ident) && tok().text == "__any")
					{
						++i;
						e.kind = ElementKind::Any;
						e.node = nullptr;
					}
					else if (at(TokKind::Ident) && tok().text == "__none")
					{
						++i;
						e.kind = ElementKind::Empty;
						e.node = nullptr;
					}
					else
					{
						error = "unexpected token '" + tok().text + "' in sequence of '" + node->name + "'" + errLine();
						return false;
					}
					elems.push_back(e);
				}
				return true;
			}

			bool parseTerminatorDecl()
			{
				// at String
				std::vector<std::string> lits;
				while (true)
				{
					if (!at(TokKind::String))
					{
						error = "expected literal" + errLine();
						return false;
					}
					lits.push_back(tok().text);
					++i;
					if (!take(TokKind::Comma))
						break;
				}

				if (!take(TokKind::Assign))
				{
					error = "expected '='" + errLine();
					return false;
				}
				if (!(at(TokKind::Ident) && tok().text == "__terminator"))
				{
					error = "expected '__terminator'" + errLine();
					return false;
				}
				++i;
				if (!take(TokKind::Semicolon))
				{
					error = "expected ';'" + errLine();
					return false;
				}

				if (!data->terminatorLiterals.empty())
				{
					error = "terminator is already declared" + errLine();
					return false;
				}
				data->terminatorLiterals = lits;
				return true;
			}

			bool parseBlock(const std::vector<Guard>& outer)
			{
				// at '{'
				++i;
				while (!at(TokKind::RBrace) && !at(TokKind::End))
				{
					std::vector<Guard> guards = outer;
					while (at(TokKind::LBracket))
					{
						Guard g;
						if (!parseGuard(g))
							return false;
						guards.push_back(g);
					}
					if (at(TokKind::LBrace))
					{
						if (!parseBlock(guards))
							return false;
					}
					else if (!parseRuleDef(guards))
						return false;
				}
				if (!take(TokKind::RBrace))
				{
					error = "expected '}'" + errLine();
					return false;
				}
				return true;
			}

			bool parseRuleDef(const std::vector<Guard>& guards)
			{
				std::string path;
				if (!parseNodeRef(path))
					return false;

				Node* node = resolveNode(path);

				Alternative a;
				if (at(TokKind::Colon))
				{
					++i;
					a.kind = AltKind::Sequence;
					if (!parseSequence(node, a.elems, TokKind::Semicolon))
						return false;
					if (!a.elems.empty())
					{
						Element& last = a.elems.back();
						if (last.kind == ElementKind::Field && last.node == node)
							last.kind = ElementKind::Optional;
					}
				}
				else if (at(TokKind::Assign) || at(TokKind::PlusAssign))
				{
					++i;
					if (at(TokKind::String) || at(TokKind::Number) ||
						(at(TokKind::Ident) && (tok().text == "true" || tok().text == "false")))
					{
						a.kind = AltKind::LiteralSet;
						while (true)
						{
							Literal lit;
							if (!parseLiteral(lit))
								return false;
							a.literals.push_back(lit);
							if (!take(TokKind::Comma))
								break;
						}
					}
					else
					{
						a.kind = AltKind::Sequence;
						if (!parseSequence(node, a.elems, TokKind::Semicolon))
							return false;
						if (!a.elems.empty())
						{
							Element& last = a.elems.back();
							if (last.kind == ElementKind::Field && last.node == node)
								last.kind = ElementKind::Optional;
						}
					}
				}
				else
				{
					error = "expected ':' or '='" + errLine();
					return false;
				}

				if (!take(TokKind::Semicolon))
				{
					error = "expected ';'" + errLine();
					return false;
				}

				a.guards = guards;
				node->alts.push_back(a);
				return true;
			}

			static void collectRefs(Element& e, std::map<Node*, bool>& refs)
			{
				switch (e.kind)
				{
				case ElementKind::Field:
					refs[e.node] = true;
					break;
				case ElementKind::Optional:
					for (std::vector<Element>::iterator it = e.inner.begin(); it != e.inner.end(); ++it)
						collectRefs(*it, refs);
					break;
				default:
					break;
				}
			}

			static void firstElemNode(Element& e, std::vector<Node*>& out)
			{
				switch (e.kind)
				{
				case ElementKind::Field:
					out.push_back(e.node);
					break;
				case ElementKind::Optional:
					if (!e.inner.empty())
						firstElemNode(e.inner[0], out);
					break;
				default:
					break;
				}
			}

			static bool dfsCycle(Node* n, std::map<Node*, std::vector<Node*> >& edges, std::map<Node*, buint8>& color, Node*& bad)
			{
				color[n] = 1;
				std::vector<Node*>& es = edges[n];
				for (std::vector<Node*>::iterator it = es.begin(); it != es.end(); ++it)
				{
					std::map<Node*, buint8>::iterator c = color.find(*it);
					if (c == color.end())
					{
						if (!dfsCycle(*it, edges, color, bad))
							return false;
					}
					else if (c->second == 1)
					{
						bad = *it;
						return false;
					}
				}
				color[n] = 2;
				return true;
			}

			bool validate()
			{
				if (data->roots.empty())
				{
					error = "no grammar rules defined";
					return false;
				}

				data->entry = data->roots[0];

				std::map<Node*, bool> referenced;
				referenced[data->entry] = true;

				for (std::map<std::string, Node>::iterator it = data->nodes.begin(); it != data->nodes.end(); ++it)
				{
					Node* n = &it->second;
					for (std::vector<Alternative>::iterator ait = n->alts.begin(); ait != n->alts.end(); ++ait)
					{
						for (std::vector<Guard>::iterator git = ait->guards.begin(); git != ait->guards.end(); ++git)
							referenced[git->node] = true;
						for (std::vector<Element>::iterator eit = ait->elems.begin(); eit != ait->elems.end(); ++eit)
							collectRefs(*eit, referenced);
					}
				}

				for (std::vector<Binding>::iterator it = data->bindings.begin(); it != data->bindings.end(); ++it)
					referenced[it->node] = true;

				for (std::map<Node*, bool>::iterator it = referenced.begin(); it != referenced.end(); ++it)
				{
					if (it->first->alts.empty())
					{
						error = "no rules defined for '" + it->first->name + "'";
						return false;
					}
				}

				std::map<Node*, std::vector<Node*> > edges;
				for (std::map<std::string, Node>::iterator it = data->nodes.begin(); it != data->nodes.end(); ++it)
				{
					Node* n = &it->second;
					for (std::vector<Alternative>::iterator ait = n->alts.begin(); ait != n->alts.end(); ++ait)
					{
						if (ait->kind != AltKind::Sequence || ait->elems.empty())
							continue;
						firstElemNode(ait->elems[0], edges[n]);
					}
				}

				std::map<Node*, buint8> color;
				Node* bad = nullptr;
				for (std::map<std::string, Node>::iterator it = data->nodes.begin(); it != data->nodes.end(); ++it)
				{
					if (color.find(&it->second) == color.end())
					{
						if (!dfsCycle(&it->second, edges, color, bad))
						{
							error = "left recursion detected at '" + bad->name + "'";
							return false;
						}
					}
				}
				return true;
			}
		};

		// ============================================================
		// input tokenization
		// ============================================================

		struct InTok
		{
			std::string text;
			buint32 offset;
			buint32 line;
			buint32 col;
		};

		static void tokenize(const std::string& text, std::vector<InTok>& out)
		{
			const buint32 n = (buint32)text.size();
			buint32 i = 0;
			buint32 line = 1;
			buint32 col = 1;
			while (i < n)
			{
				while (i < n && std::isspace((unsigned char)text[i]))
				{
					if (text[i] == '\n')
					{
						++line;
						col = 1;
					}
					else
					{
						++col;
					}
					++i;
				}
				if (i >= n)
					break;
				InTok t;
				t.offset = i;
				t.line = line;
				t.col = col;
				while (i < n && !std::isspace((unsigned char)text[i]))
				{
					t.text += text[i];
					++i;
					++col;
				}
				out.push_back(t);
			}
		}

		// ============================================================
		// matching runtime
		// ============================================================

		static const buint32 maxMatchDepth = 1024;

		typedef std::map<Node*, Value> CaptureMap;

		struct MatchState
		{
			const std::vector<InTok>* tokens;
			const std::vector<std::string>* terms;
			buint32 pos;
			buint32 depth;
			CaptureMap captures;

			MatchState()
				: tokens(nullptr)
				, terms(nullptr)
				, pos(0)
				, depth(0)
			{
			}
		};

		struct MatchResult
		{
			bool ok;
			buint32 consumed;
			MatchState state;
			Value value;
			std::string error;
		};

		static void matchNode(Node* node, const MatchState& st, MatchResult& res);

		static std::string guardFailReason(const Alternative& alt, const CaptureMap& captures)
		{
			for (std::vector<Guard>::const_iterator it = alt.guards.begin(); it != alt.guards.end(); ++it)
			{
				CaptureMap::const_iterator c = captures.find(it->node);
				if (c == captures.end())
					return "guard '::" + it->node->name + " == \"" + it->text + "\"' failed: not captured";
				if (c->second.str != it->text)
				{
					std::string got = (c->second.type == ValueType::AnyList)
						? "list of " + toStr(c->second.list.size()) + " value(s)"
						: "'" + c->second.str + "'";
					return "guard '::" + it->node->name + " == \"" + it->text + "\"' failed: captured " + got;
				}
			}
			return std::string();
		}

		static void pushUniqueReason(std::vector<std::string>& list, const std::string& r, buint32& skipped)
		{
			for (std::vector<std::string>::const_iterator it = list.begin(); it != list.end(); ++it)
			{
				if (*it == r)
					return;
			}
			if (list.size() < 3)
				list.push_back(r);
			else
				++skipped;
		}

		static void indentLines(std::string& s)
		{
			std::string::size_type pos = 0;
			while ((pos = s.find('\n', pos)) != std::string::npos)
			{
				s.insert(pos + 1, "    ");
				pos += 5;
			}
		}

		static void pushFlatten(Value& dst, const Value& src)
		{
			if (src.type == ValueType::AnyList)
			{
				for (std::vector<Value>::const_iterator it = src.list.begin(); it != src.list.end(); ++it)
					dst.list.push_back(*it);
			}
			else if (src.type != ValueType::None)
			{
				dst.list.push_back(src);
			}
		}

		static bool isTermToken(const MatchState& st, const std::string& text)
		{
			if (st.terms == nullptr)
				return false;
			for (std::vector<std::string>::const_iterator it = st.terms->begin(); it != st.terms->end(); ++it)
			{
				if (text == *it)
					return true;
			}
			return false;
		}

		static std::string describeTok(const std::vector<InTok>& tokens, buint32 pos)
		{
			if (pos >= tokens.size())
				return "end of input";
			const InTok& t = tokens[pos];
			return "token " + toStr(pos + 1) + "/" + toStr(tokens.size()) + " '" + t.text +
				"' (byte offset " + toStr(t.offset) + ", line " + toStr(t.line) + ":" + toStr(t.col) + ")";
		}

		static std::string describePos(const MatchState& st, buint32 pos)
		{
			return describeTok(*st.tokens, pos);
		}

		static std::string valueTypeName(ValueType t)
		{
			switch (t)
			{
			case ValueType::None: return "none";
			case ValueType::String: return "string";
			case ValueType::Int: return "int";
			case ValueType::Float: return "float";
			case ValueType::Bool: return "bool";
			case ValueType::AnyList: return "list";
			}
			return "unknown";
		}

		static std::string varTypeName(VarType t)
		{
			switch (t)
			{
			case VarType::String: return "string";
			case VarType::Int: return "int";
			case VarType::Float: return "float";
			case VarType::Bool: return "bool";
			case VarType::ArrayString: return "array<string>";
			case VarType::ArrayInt: return "array<int>";
			case VarType::ArrayFloat: return "array<float>";
			case VarType::ArrayBool: return "array<bool>";
			}
			return "unknown";
		}

		static bool matchElem(Element& e, MatchState& st, Value& out, std::string& error, buint32& consumed, const std::string& nodeName)
		{
			consumed = 0;

			switch (e.kind)
			{
			case ElementKind::Terminator:
			{
				if (st.pos != st.tokens->size())
				{
					if (!isTermToken(st, (*st.tokens)[st.pos].text))
					{
						if (st.terms->empty())
						{
							error = "expected end of input at '::" + nodeName + "'\n  but got '" +
								(*st.tokens)[st.pos].text + "' (" + describePos(st, st.pos) + ")";
						}
						else
						{
							std::string terms;
							for (std::vector<std::string>::const_iterator it = st.terms->begin(); it != st.terms->end(); ++it)
							{
								if (!terms.empty())
									terms += ", ";
								terms += "'" + *it + "'";
							}
							error = "expected terminator " + terms + " at '::" + nodeName + "'\n  but got '" +
								(*st.tokens)[st.pos].text + "' (" + describePos(st, st.pos) + ")";
						}
						return false;
					}
					++st.pos;
					consumed = 1;
				}
				out.type = ValueType::None;
				return true;
			}

			case ElementKind::Everything:
			{
				Value v;
				v.type = ValueType::AnyList;
				for (buint32 k = st.pos; k < st.tokens->size() && !isTermToken(st, (*st.tokens)[k].text); ++k)
				{
					Value t;
					t.type = ValueType::String;
					t.str = (*st.tokens)[k].text;
					v.list.push_back(t);
				}
				consumed = (buint32)v.list.size();
				st.pos += consumed;
				out = v;
				return true;
			}

			case ElementKind::Any:
			{
				if (st.pos < st.tokens->size() && !isTermToken(st, (*st.tokens)[st.pos].text))
				{
					out.type = ValueType::String;
					out.str = (*st.tokens)[st.pos].text;
					++st.pos;
					consumed = 1;
					return true;
				}
				if (st.pos >= st.tokens->size())
					error = "expected any token at '::" + nodeName + "'\n  but found end of input";
				else
					error = "expected any token at '::" + nodeName + "'\n  but found terminator '" +
						(*st.tokens)[st.pos].text + "' (" + describePos(st, st.pos) + ")";
				return false;
			}

			case ElementKind::Empty:
			{
				consumed = 0;
				out.type = ValueType::None;
				return true;
			}

			case ElementKind::Field:
			{
				MatchState child;
				child.tokens = st.tokens;
				child.terms = st.terms;
				child.pos = st.pos;
				child.depth = st.depth;
				child.captures = st.captures;

				MatchResult r;
				matchNode(e.node, child, r);
				if (!r.ok)
				{
					error = r.error;
					st.pos = r.state.pos;
					consumed = r.consumed;
					return false;
				}
				st.pos = r.state.pos;
				for (CaptureMap::iterator cit = r.state.captures.begin(); cit != r.state.captures.end(); ++cit)
					st.captures[cit->first] = cit->second;
				st.captures[e.node] = r.value;
				out = r.value;
				consumed = r.consumed;
				return true;
			}

			case ElementKind::Optional:
			{
				MatchState innerSt = st;
				std::vector<Value> innerVals;
				for (std::vector<Element>::iterator it = e.inner.begin(); it != e.inner.end(); ++it)
				{
					buint32 ec = 0;
					Value v;
					std::string innerErr;
					if (!matchElem(*it, innerSt, v, innerErr, ec, nodeName))
					{
						consumed += ec;
						if (consumed == 0)
						{
							// nothing matched -> skip silently
							out.type = ValueType::None;
							return true;
						}
						st = innerSt;
						error = innerErr;
						return false;
					}
					consumed += ec;
					if (v.type != ValueType::None)
						innerVals.push_back(v);
				}
				st = innerSt;
				Value v;
				v.type = ValueType::AnyList;
				for (std::vector<Value>::iterator it = innerVals.begin(); it != innerVals.end(); ++it)
					pushFlatten(v, *it);
				out = v;
				return true;
			}
			}

			return false;
		}

		static bool matchAlt(Alternative& alt, MatchState& st, Value& out, std::string& error, const std::string& nodeName)
		{
			if (alt.kind == AltKind::LiteralSet)
			{
				std::string expected;
				for (std::vector<Literal>::iterator it = alt.literals.begin(); it != alt.literals.end(); ++it)
				{
					if (it != alt.literals.begin())
						expected += ", ";
					expected += "'" + it->text + "'";
				}

				if (st.pos >= st.tokens->size())
				{
					error = "expected " + expected + " at '::" + nodeName + "'\n  but input ended";
					return false;
				}

				const std::string& text = (*st.tokens)[st.pos].text;
				for (std::vector<Literal>::iterator it = alt.literals.begin(); it != alt.literals.end(); ++it)
				{
					if (text == it->text)
					{
						Value v;
						v.type = it->type;
						v.str = text;
						switch (it->type)
						{
						case ValueType::Int:
							v.i = (bint32)std::strtol(text.c_str(), nullptr, 10);
							break;
						case ValueType::Float:
							v.f = (float)std::atof(text.c_str());
							break;
						case ValueType::Bool:
							v.b = (text == "true");
							break;
						default:
							break;
						}
						++st.pos;
						out = v;
						return true;
					}
				}

				error = "expected " + expected + " at '::" + nodeName + "'\n  but got '" + text + "' (" + describePos(st, st.pos) + ")";
				return false;
			}

			std::vector<Value> seq;
			for (std::vector<Element>::iterator eit = alt.elems.begin(); eit != alt.elems.end(); ++eit)
			{
				buint32 ec = 0;
				Value v;
				if (!matchElem(*eit, st, v, error, ec, nodeName))
					return false;
				if (v.type != ValueType::None)
					seq.push_back(v);
			}

			if (seq.size() == 1)
			{
				out = seq[0];
				return true;
			}

			Value v;
			v.type = ValueType::AnyList;
			for (std::vector<Value>::iterator it = seq.begin(); it != seq.end(); ++it)
				pushFlatten(v, *it);
			out = v;
			return true;
		}

		static void matchNode(Node* node, const MatchState& st, MatchResult& res)
		{
			res.ok = false;
			res.consumed = 0;
			res.error.clear();

			if (st.depth >= maxMatchDepth)
			{
				res.error = std::string("recursion depth exceeded (possible left recursion) at '") + node->name + "'";
				return;
			}

			MatchState start = st;
			res.state = start;

			std::vector<std::string> matchReasons;
			std::vector<std::string> guardReasons;
			buint32 skippedReasons = 0;

			for (std::vector<Alternative>::iterator ait = node->alts.begin(); ait != node->alts.end(); ++ait)
			{
				MatchState a = start;
				++a.depth;

				std::string gfail = guardFailReason(*ait, a.captures);
				if (!gfail.empty())
				{
					pushUniqueReason(guardReasons, "alternative " + toStr(ait - node->alts.begin() + 1) + ": " + gfail, skippedReasons);
					continue;
				}

				Value v;
				std::string altErr;
				bool ok = matchAlt(*ait, a, v, altErr, node->name);

				buint32 consumed = a.pos - start.pos;
				if (consumed > res.consumed)
				{
					res.consumed = consumed;
					res.state = a;
				}

				if (ok)
				{
					res.ok = true;
					res.consumed = consumed;
					res.state = a;
					res.value = v;
					return;
				}

				if (consumed > 0)
				{
					// greedy commit: the first alternative that started matching fails the node
					res.error = altErr.empty()
						? "alternative at '::" + node->name + "' failed after consuming " + toStr(consumed) + " token(s)"
						: altErr + "\n  alternative at '::" + node->name + "' failed after consuming " + toStr(consumed) + " token(s)";
					return;
				}

				if (!altErr.empty())
				{
					std::string r = "alternative " + toStr(ait - node->alts.begin() + 1) + ": " + altErr;
					indentLines(r);
					pushUniqueReason(matchReasons, r, skippedReasons);
				}
			}

			std::vector<std::string> reasons;
			for (std::vector<std::string>::const_iterator it = matchReasons.begin(); it != matchReasons.end(); ++it)
			{
				if (reasons.size() < 4)
					reasons.push_back(*it);
				else
					++skippedReasons;
			}
			for (std::vector<std::string>::const_iterator it = guardReasons.begin(); it != guardReasons.end(); ++it)
			{
				if (reasons.size() < 4)
					reasons.push_back(*it);
				else
					++skippedReasons;
			}

			res.error = "no alternative matched at '::" + node->name + "'";
			if (reasons.empty() && skippedReasons == 0)
				res.error += " (node has no alternatives)";
			for (std::vector<std::string>::const_iterator it = reasons.begin(); it != reasons.end(); ++it)
				res.error += "\n  " + *it;
			if (skippedReasons > 0)
				res.error += "\n  ... and " + toStr(skippedReasons) + " more alternative(s)";
		}

		// ============================================================
		// conversions
		// ============================================================

		static bool strictInt(const std::string& s, bint32& out)
		{
			if (s.empty())
				return false;
			char* end = nullptr;
			long v = std::strtol(s.c_str(), &end, 10);
			if (end == s.c_str() || *end != '\0')
				return false;
			if (v < INT32_MIN || v > INT32_MAX)
				return false;
			out = (bint32)v;
			return true;
		}

		static bool strictFloat(const std::string& s, float& out)
		{
			if (s.empty())
				return false;
			char* end = nullptr;
			float v = std::strtof(s.c_str(), &end);
			if (end == s.c_str() || *end != '\0')
				return false;
			out = v;
			return true;
		}

		static VarType scalarTypeOf(VarType vt)
		{
			switch (vt)
			{
			case VarType::ArrayString: return VarType::String;
			case VarType::ArrayInt: return VarType::Int;
			case VarType::ArrayFloat: return VarType::Float;
			case VarType::ArrayBool: return VarType::Bool;
			default: return vt;
			}
		}

		static bool valueToVarType(const Value& v, VarType vt, Value& out)
		{
			switch (vt)
			{
			case VarType::String:
				if (v.type != ValueType::String && v.type != ValueType::Int && v.type != ValueType::Float && v.type != ValueType::Bool)
					return false;
				out.type = ValueType::String;
				out.str = v.str;
				return true;

			case VarType::Int:
				if (v.type == ValueType::Int)
				{
					out.type = ValueType::Int;
					out.str = v.str;
					out.i = v.i;
					return true;
				}
				if (v.type == ValueType::String)
				{
					bint32 x;
					if (!strictInt(v.str, x))
						return false;
					out.type = ValueType::Int;
					out.str = v.str;
					out.i = x;
					return true;
				}
				return false;

			case VarType::Float:
				if (v.type == ValueType::Float)
				{
					out.type = ValueType::Float;
					out.str = v.str;
					out.f = v.f;
					return true;
				}
				if (v.type == ValueType::Int)
				{
					out.type = ValueType::Float;
					out.str = v.str;
					out.f = (float)v.i;
					return true;
				}
				if (v.type == ValueType::String)
				{
					float x;
					if (!strictFloat(v.str, x))
						return false;
					out.type = ValueType::Float;
					out.str = v.str;
					out.f = x;
					return true;
				}
				return false;

			case VarType::Bool:
				if (v.type == ValueType::Bool)
				{
					out.type = ValueType::Bool;
					out.str = v.str;
					out.b = v.b;
					return true;
				}
				if (v.type == ValueType::String)
				{
					if (v.str == "true")
					{
						out.type = ValueType::Bool;
						out.str = v.str;
						out.b = true;
						return true;
					}
					if (v.str == "false")
					{
						out.type = ValueType::Bool;
						out.str = v.str;
						out.b = false;
						return true;
					}
				}
				return false;

			case VarType::ArrayString:
			case VarType::ArrayInt:
			case VarType::ArrayFloat:
			case VarType::ArrayBool:
			{
				if (v.type != ValueType::AnyList)
					return false;
				out.type = ValueType::AnyList;
				VarType elemType = scalarTypeOf(vt);
				for (std::vector<Value>::const_iterator it = v.list.begin(); it != v.list.end(); ++it)
				{
					Value converted;
					if (!valueToVarType(*it, elemType, converted))
						return false;
					out.list.push_back(converted);
				}
				return true;
			}
			}

			return false;
		}

		// ============================================================
		// Parser
		// ============================================================

		static bool assignBindings(std::vector<Binding>& bindings, std::string& error, const CaptureMap& captures)
		{
			for (std::vector<Binding>::iterator it = bindings.begin(); it != bindings.end(); ++it)
			{
				Binding& b = *it;
				CaptureMap::const_iterator c = captures.find(b.node);
				if (c == captures.end())
				{
					switch (b.var->type)
					{
					case VarType::ArrayString:
					case VarType::ArrayInt:
					case VarType::ArrayFloat:
					case VarType::ArrayBool:
					{
						Value empty;
						empty.type = ValueType::AnyList;
						b.var->value = empty;
						break;
					}
					default:
						break;
					}
					continue;
				}

				b.var->captured = true;

				if (b.var->type == VarType::String || b.var->type == VarType::Int ||
					b.var->type == VarType::Float || b.var->type == VarType::Bool)
				{
					if (c->second.type == ValueType::AnyList && c->second.list.empty())
						continue;
				}

				Value converted;
				if (!valueToVarType(c->second, b.var->type, converted))
				{
					error = "cannot assign captured value of type '" + valueTypeName(c->second.type) +
						"' to variable '" + b.var->name + "' of type '" + varTypeName(b.var->type) +
						"' (binding '::" + b.node->name + "')";
					return false;
				}
				b.var->value = converted;
			}
			return true;
		}

		Parser::Parser()
		{
			d = new Data();
		}

		Parser::~Parser()
		{
			delete d;
		}

		bool Parser::load(const std::string& path)
		{
			d->clearGrammar();

			std::ifstream file(path.c_str(), std::ios::binary);
			if (!file.is_open())
			{
				d->error = "cannot open file '" + path + "'";
				return false;
			}

			std::string src((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			file.close();

			std::vector<Token> tokens;
			if (!lex(src, tokens, d->error))
			{
				d->error = "pdl: " + d->error;
				return false;
			}

			Loader l;
			l.toks = &tokens;
			l.i = 0;
			l.data = d;

			if (!l.parseProgram())
			{
				d->error = "pdl: " + l.error;
				return false;
			}
			if (!l.validate())
			{
				d->error = "pdl: " + l.error;
				return false;
			}
			return true;
		}

		bool Parser::parse(const std::string& text)
		{
			d->error.clear();
			d->resetVars();

			if (d->entry == nullptr)
			{
				d->error = "no grammar loaded";
				return false;
			}

			std::vector<InTok> tokens;
			tokenize(text, tokens);

			MatchState st;
			st.tokens = &tokens;
			st.terms = &d->terminatorLiterals;

			MatchResult res;
			matchNode(d->entry, st, res);

			if (!res.ok)
			{
				d->error = "parse error";
				if (!res.error.empty())
					d->error += ": " + res.error;
				return false;
			}

			if (res.state.pos != tokens.size())
			{
				buint32 left = (buint32)tokens.size() - res.state.pos;
				d->error = "unexpected trailing input at '::" + d->entry->name + "': " + toStr(left) + " token(s) left";
				d->error += "\n  first leftover: " + describeTok(tokens, res.state.pos);
				if (left > 1)
				{
					std::string rest;
					buint32 maxK = tokens.size() < res.state.pos + 4 ? (buint32)tokens.size() : res.state.pos + 4;
					for (buint32 k = res.state.pos + 1; k < maxK; ++k)
					{
						if (!rest.empty())
							rest += ", ";
						rest += "'" + tokens[k].text + "'";
					}
					d->error += "\n  rest: " + rest + (maxK < tokens.size() ? ", ..." : "");
				}
				return false;
			}

			return assignBindings(d->bindings, d->error, res.state.captures);
		}

		bool Parser::parseNext(const std::string& text, size_t& offset)
		{
			d->error.clear();
			d->resetVars();

			if (d->entry == nullptr)
			{
				d->error = "no grammar loaded";
				return false;
			}

			std::vector<InTok> tokens;
			tokenize(text, tokens);

			buint32 startIdx = 0;
			while (startIdx < tokens.size() && tokens[startIdx].offset < offset)
				++startIdx;

			if (startIdx >= tokens.size())
				return false;

			MatchState st;
			st.tokens = &tokens;
			st.pos = startIdx;
			st.terms = &d->terminatorLiterals;

			MatchResult res;
			matchNode(d->entry, st, res);

			if (!res.ok)
			{
				d->error = "parse error";
				if (!res.error.empty())
					d->error += ": " + res.error;
				return false;
			}

			if (res.state.pos <= startIdx && res.state.pos < tokens.size())
			{
				d->error = "zero-length match at '::" + d->entry->name + "' (" + describeTok(tokens, startIdx) + ")";
				return false;
			}

			if (res.state.pos >= tokens.size())
				offset = text.size();
			else
				offset = tokens[res.state.pos].offset;

			return assignBindings(d->bindings, d->error, res.state.captures);
		}

		const std::string& Parser::getLastError() const
		{
			return d->error;
		}

		bool Parser::isCaptured(const std::string& name) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = d->vars.find(name);
			if (it == d->vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			return it->second.captured;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, std::string& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::String)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::String);
				return false;
			}
			if (it->second.value.type != ValueType::String)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out = it->second.value.str;
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, bint32& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::Int)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::Int);
				return false;
			}
			if (it->second.value.type != ValueType::Int)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out = it->second.value.i;
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, float& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::Float)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::Float);
				return false;
			}
			if (it->second.value.type != ValueType::Float)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out = it->second.value.f;
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, bool& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::Bool)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::Bool);
				return false;
			}
			if (it->second.value.type != ValueType::Bool)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out = it->second.value.b;
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, std::vector<std::string>& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::ArrayString)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::ArrayString);
				return false;
			}
			if (it->second.value.type != ValueType::AnyList)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out.clear();
			for (std::vector<Value>::const_iterator vit = it->second.value.list.begin(); vit != it->second.value.list.end(); ++vit)
			{
				if (vit->type != ValueType::String)
				{
					d->error = "variable '" + name + "' contains non-string element";
					return false;
				}
				out.push_back(vit->str);
			}
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, std::vector<bint32>& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::ArrayInt)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::ArrayInt);
				return false;
			}
			if (it->second.value.type != ValueType::AnyList)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out.clear();
			for (std::vector<Value>::const_iterator vit = it->second.value.list.begin(); vit != it->second.value.list.end(); ++vit)
			{
				if (vit->type != ValueType::Int)
				{
					d->error = "variable '" + name + "' contains non-int element";
					return false;
				}
				out.push_back(vit->i);
			}
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, std::vector<float>& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::ArrayFloat)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::ArrayFloat);
				return false;
			}
			if (it->second.value.type != ValueType::AnyList)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out.clear();
			for (std::vector<Value>::const_iterator vit = it->second.value.list.begin(); vit != it->second.value.list.end(); ++vit)
			{
				if (vit->type != ValueType::Float)
				{
					d->error = "variable '" + name + "' contains non-float element";
					return false;
				}
				out.push_back(vit->f);
			}
			return true;
		}

		bool Parser::getVarImpl(const Data& data, const std::string& name, std::vector<bool>& out) const
		{
			d->error.clear();
			std::map<std::string, Var>::const_iterator it = data.vars.find(name);
			if (it == data.vars.end())
			{
				d->error = "variable '" + name + "' is not declared in the grammar";
				return false;
			}
			if (it->second.type != VarType::ArrayBool)
			{
				d->error = "variable '" + name + "' has type " + varTypeName(it->second.type) + ", requested " + varTypeName(VarType::ArrayBool);
				return false;
			}
			if (it->second.value.type != ValueType::AnyList)
			{
				d->error = "variable '" + name + "' was not captured";
				return false;
			}
			out.clear();
			for (std::vector<Value>::const_iterator vit = it->second.value.list.begin(); vit != it->second.value.list.end(); ++vit)
			{
				if (vit->type != ValueType::Bool)
				{
					d->error = "variable '" + name + "' contains non-bool element";
					return false;
				}
				out.push_back(vit->b);
			}
			return true;
		}
	}
}
