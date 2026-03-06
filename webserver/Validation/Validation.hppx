#pragma once
#include "../Headers.hpp"

enum ReturnKind
{
    RETURN_NONE,
    RETURN_200,
    RETURN_REDIRECT
};
class Validation
{
	public:
		Validation(AST<string> &astRoot);
		void Validate();

	private:
		AST<string> &_root;

		struct ServerSeen
		{
			bool listen;
			bool serverName;
			bool root;
			bool index;
			bool autoindex;
			bool returnDir;
			bool clientMaxBodySize;
			bool allowMethods;
			bool errorPage;

			ServerSeen() : listen(false), serverName(false), root(false),
						   index(false), autoindex(false), returnDir(false),
						   clientMaxBodySize(false), allowMethods(false),
						   errorPage(false) {}
		};

		struct LocationSeen
		{
			bool root;
			bool index;
			bool autoindex;
			bool returnDir;
			bool clientMaxBodySize;
			bool allowMethods;
			bool cgiPass;
			bool bodyInFile;
			bool deleteFiles;
			ReturnKind returnKind;

			LocationSeen() : root(false), index(false), autoindex(false),
							 returnDir(false), clientMaxBodySize(false),
							 allowMethods(false), cgiPass(false),
							 bodyInFile(false), deleteFiles(false), returnKind(RETURN_NONE) {}
		};

		// --- top-level validators ---
		void validateServer(AST<string> &serverNode);
		void validateLocation(AST<string> &locationNode, LocationSeen &seen);

		// --- directive validators (server scope) ---
		void validateListen(AST<string> &node, ServerSeen &seen);
		void validateServerName(AST<string> &node, ServerSeen &seen);
		void validateRoot(AST<string> &node, bool &seen);
		void validateIndex(AST<string> &node, bool &seen);
		void validateAutoindex(AST<string> &node, bool &seen);
		void validateReturn(AST<string> &node, bool &seen);
		void validateClientMaxBody(AST<string> &node, bool &seen);
		void validateAllowMethods(AST<string> &node, bool &seen);
		void validateErrorPage(AST<string> &node, bool &seen);
		

		// --- directive validators (location-only scope) ---
		void validateCgiPass(AST<string> &node, LocationSeen &seen);
		void validateBodyInFile(AST<string> &node, LocationSeen &seen);
		void validateDeleteFiles(AST<string> &node, LocationSeen &seen);
		void validateReturn(AST<string> &node, LocationSeen &seen);

		// --- helpers ---
		ReturnKind classifyReturnCode(long code);
		static long parseNumber(const string &s);
		static bool isByteSizeUnit(char c);
		static bool isValidMethod(const string &m);
		static void checkArgCount(AST<string> &node, size_t min, size_t max, const string &name);
		static void requireArgs(AST<string> &node, const string &name);
};
