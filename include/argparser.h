#pragma once
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace Argparser
{

	static bool is_number(const std::string& s)
	{
		std::string::const_iterator it = s.begin();
		while(it != s.end() && std::isdigit(*it))
			++it;
		return !s.empty() && it == s.end();
	}

	enum ArgumentType
	{
		str, // String
		num, // int64_t number
		def, // No type, just definition
	};

	struct ArgumentDescriptor
	{
		ArgumentDescriptor(int id,
											 std::string_view name,
											 std::string_view description = "",
											 bool is_required							= true,
											 bool is_sequential						= true,
											 ArgumentType type						= ArgumentType::str)
				: id(id), name(name), description(description), is_required(is_required),
					is_positional(is_sequential), type(type)
		{
			// if(is_sequential && !is_required)
			// {
			// 	std::cerr << std::format("Argument {} cant be required and not be positional", name)
			// 						<< std::endl;
			// 	exit(EXIT_FAILURE);
			// }
			switch(type)
			{
				case str:
					value = malloc(sizeof(std::string));
					break;
				case num:
					value = malloc(sizeof(int64_t));
					break;
				case def:
					break;
			}
		};
		void removeValue()
		{

			if(value != nullptr)
			{
				switch(type)
				{
					case str:
						delete(std::string*)(value);
						break;
					case num:
						delete(uint64_t*)(value);
						break;
					case def:
						break;
					default:
						std::cerr << "Achtung! Memory leak due to unknown argument type! " << std::endl;
				}
			}
		}

		int id;
		std::string_view name;
		std::string_view description;
		void* value = nullptr;
		bool is_required;
		bool is_positional;
		ArgumentType type;

		bool was_defined = false;
	};
	std::ostream& operator<<(std::ostream& os, const ArgumentDescriptor ad)
	{
		if(ad.is_positional)
		{
			os << ad.id + 1 << ": ";
		}
		if(!ad.description.empty())
		{
			os << std::format("{}: {}", ad.name, ad.description);
		}
		else
		{
			return os << ad.name;
		}

		return os;
	};

	class Argparser
	{
	public:
		Argparser(int& argc, char* argv[]) : m_argc(argc), m_argv(argv) {};
		~Argparser()
		{
			for(auto a : m_arguments)
			{
				a.removeValue();
			}
		}

		void setProgramName(std::string_view name) { m_name = name; }
		void addArgument(std::string_view name,
										 std::string_view description = "",
										 bool is_required							= true,
										 bool is_positional						= true,
										 ArgumentType type						= ArgumentType::str)
		{

			m_arguments.emplace_back(m_positional_args, name, description, is_required, is_positional,
															 type);
			if(is_positional)
			{
				m_positional_args++;
			}
		}
		void printHelp()
		{
			std::cout << std::format("Help for program {}:\n\n", m_name) //
								<< "Required arguments:" << std::endl;
			for(auto& arg : m_arguments)
			{
				if(arg.is_required)
				{
					std::cout << "    " << arg << std::endl;
				}
			}
			std::cout << "\nOptional arguments:" << std::endl;
			for(auto& arg : m_arguments)
			{
				if(!arg.is_required)
				{
					std::cout << "    " << arg << std::endl;
				}
			}
		};
		void dumpArguments()
		{
			std::cout << "Argument dump:" << std::endl;
			for(auto& arg : m_arguments)
			{
				if(arg.was_defined)
				{
					switch(arg.type)
					{
						case str:
							std::cout << std::format("{}: {}", arg.name, *((std::string*)arg.value)) << std::endl;
							break;
						case num:
							std::cout << std::format("{}: {}", arg.name, *((std::int64_t*)arg.value))
												<< std::endl;
							break;
						case def:
							break;
					}
				}
				else
				{
					std::cout << std::format("{} was not defined", arg.name) << std::endl;
				}
			}
		}

		void parse()
		{
			struct SeachArgs
			{
				std::string n;
				bool was_used = false;
			};
			std::vector<SeachArgs> args;
			// Copy argv to vector for simpler processing
			args.reserve(m_argc);
			// Start i from 1 to exclude launch command (And also display help lul)
			for(int i = 1; i < m_argc; i++)
			{
				args.emplace_back(m_argv[i], false);
				if(args.at(i - 1).n == "-h" || args.at(i - 1).n == "--help")
				{
					printHelp();
					exit(EXIT_SUCCESS);
				};
			}

			// Handling of unordered arguments
			for(int i = 0; i < args.size(); i++)
			{
				bool islast					= (i + 1) >= args.size();
				std::string& argstr = args.at(i).n;
				for(auto& arg : m_arguments)
				{
					if(!arg.is_positional && argstr == arg.name)
					{

						args.at(i).was_used = true;
						arg.was_defined			= true;
						switch(arg.type)
						{
							case def:
								break;
							case str:
								if(islast)
								{
									triggerInvalidArgument();
								}
								*((std::string*)arg.value) = args.at(i + 1).n;
								args.at(i + 1).was_used		 = true;
								i++;
								break;
							case num:
								if(islast)
								{
									triggerInvalidArgument();
								}
								if(!is_number(args.at(i + 1).n))
								{
									std::cerr << std::format("{} should be number but is not", arg.name) << std::endl;
									printHelp();
									exit(EXIT_FAILURE);
								}
								*((int64_t*)arg.value)	= std::stoi(args.at(i + 1).n);
								args.at(i + 1).was_used = true;
								i++;
								break;
						}
						break;
					}
				}
			}

			// Positional arguments
			for(int i = 0; i < args.size(); i++)
			{
				bool islast = (i + 1) >= args.size();
				auto& arg_p = args.at(i);
				if(arg_p.was_used)
				{
					continue;
				}
				for(auto& arg : m_arguments)
				{
					if(arg.is_positional && !arg.was_defined)
					{
						arg.was_defined = true;
						switch(arg.type)
						{
							case def:
								break;
							case str:
								*((std::string*)arg.value) = arg_p.n;
								break;
							case num:
								if(!is_number(arg_p.n))
								{
									std::cerr << std::format("{} should be number but is not", arg.name) << std::endl;
									printHelp();
									exit(EXIT_FAILURE);
								}
								*((int64_t*)arg.value) = std::stoi(arg_p.n);
								break;
						}
						break;
					}
				}
			}

			// Check if required arguments were given
			for(auto& arg : m_arguments)
			{
				if(arg.is_required && !arg.was_defined)
				{
					std::cerr << std::format("{} has not been specified", arg.name) << std::endl;
					printHelp();
					exit(EXIT_FAILURE);
				}
			}
		};

		std::string getString(int id)
		{
			if(m_arguments.at(id).type == str)
			{
				return *(std::string*)m_arguments.at(id).value;
			}

			std::cerr << std::format("Type mismatch in {} at id {}", m_arguments.at(id).name, id)
								<< std::endl;
			exit(EXIT_FAILURE);
		}

		int64_t getNumber(int id)
		{
			if(m_arguments.at(id).type == num)
			{
				return *(int64_t*)m_arguments.at(id).value;
			}

			std::cerr << std::format("Type mismatch in {} at id {}", m_arguments.at(id).name, id)
								<< std::endl;
			exit(EXIT_FAILURE);
		}

		bool getDefined(int id) { return m_arguments.at(id).was_defined; }

	private:
		int argc() { return m_argc; };
		void triggerInvalidArgument()
		{
			std::cerr << "Invalid arguments" << std::endl;
			printHelp();
			exit(EXIT_FAILURE);
		}

		int& m_argc;
		char** m_argv;
		int m_positional_args = 0;
		std::vector<ArgumentDescriptor> m_arguments;
		std::string m_name;
		std::string_view exec_command;
	};
};