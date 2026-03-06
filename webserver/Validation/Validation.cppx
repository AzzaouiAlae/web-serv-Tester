#include "Validation.hpp"

Validation::Validation(AST<string> &astRoot)
	: _root(astRoot)
{
	DEBUG("Validation") << "Validation initialized.";
}

void Validation::Validate()
{
	vector<AST<string> > &topLevel = _root.GetChildren();

	if (topLevel.empty())
		Error::ThrowError("Config Error: no server blocks found");

	DEBUG("Validation") << "Starting AST validation: " << topLevel.size() << " top-level block(s) found.";

	for (int i = 0; i < (int)topLevel.size(); i++)
	{
		const string &kind = topLevel[i].GetValue();
		DDEBUG("Validation") << "Evaluating top-level block [" << i << "]: '" << kind << "'";

		if (kind == "server")
			validateServer(topLevel[i]);
		else if (kind == "types")
			DDEBUG("Validation") << "  Skipping 'types' block (structural only, no validation needed).";
		else
			Error::ThrowError("Config Error: unexpected top-level block '" + kind + "'");
	}
	DEBUG("Validation") << "Validation complete. All " << topLevel.size() << " block(s) passed.";
}

void Validation::validateServer(AST<string> &serverNode)
{
	ServerSeen seen;

	vector<AST<string> > &children = serverNode.GetChildren();

	DEBUG("Validation") << "--> Validating 'server' block with " << children.size() << " directive(s).";

	for (int i = 0; i < (int)children.size(); i++)
	{
		AST<string> &node = children[i];
		const string &directive = node.GetValue();

		DDEBUG("Validation") << "  [Server Block] Checking directive [" << i << "]: '" << directive << "'";

		if (directive == "listen")
			validateListen(node, seen);
		else if (directive == "server_name")
			validateServerName(node, seen);
		else if (directive == "root")
			validateRoot(node, seen.root);
		else if (directive == "index")
			validateIndex(node, seen.index);
		else if (directive == "autoindex")
			validateAutoindex(node, seen.autoindex);
		else if (directive == "return")
			validateReturn(node, seen.returnDir);
		else if (directive == "client_max_body_size")
			validateClientMaxBody(node, seen.clientMaxBodySize);
		else if (directive == "allow_methods")
			validateAllowMethods(node, seen.allowMethods);
		else if (directive == "error_page")
			validateErrorPage(node, seen.errorPage);
		else if (directive == "location")
		{
			if (node.GetArguments().empty())
				Error::ThrowError("Config Error: location block has no path");
			const string &path = node.GetArguments()[0];
			if (path.empty() || path[0] != '/')
				Error::ThrowError("Config Error: location path must start with '/' — got '" + path + "'");

			DEBUG("Validation") << "  --> Validating 'location' block: " << path;
			LocationSeen locSeen;
			validateLocation(node, locSeen);
		}
		else
			Error::ThrowError("Config Error: unknown directive '" + directive + "' in server block");
	}

	if (!seen.listen)
		Error::ThrowError("Config Error: server block is missing required 'listen' directive");
	if (!seen.serverName)
		Error::ThrowError("Config Error: server block is missing required 'server_name' directive");
	DEBUG("Validation") << "  'server' block validation passed.";
}

void Validation::validateLocation(AST<string> &locationNode, LocationSeen &seen)
{
	vector<AST<string> > &children = locationNode.GetChildren();

	DDEBUG("Validation") << "    Location block has " << children.size() << " directive(s) to validate.";

	for (int i = 0; i < (int)children.size(); i++)
	{
		AST<string> &node = children[i];
		const string &directive = node.GetValue();

		DDEBUG("Validation") << "    [Location Block] Checking directive [" << i << "]: '" << directive << "'";

		if (directive == "root")
			validateRoot(node, seen.root);
		else if (directive == "index")
			validateIndex(node, seen.index);
		else if (directive == "autoindex")
			validateAutoindex(node, seen.autoindex);
		else if (directive == "return")
			validateReturn(node, seen);
		else if (directive == "client_max_body_size")
			validateClientMaxBody(node, seen.clientMaxBodySize);
		else if (directive == "allow_methods")
			validateAllowMethods(node, seen.allowMethods);
		else if (directive == "cgi_pass")
			validateCgiPass(node, seen);
		else if (directive == "client_body_in_file_only")
			validateBodyInFile(node, seen);
		else if (directive == "delete_files")
			validateDeleteFiles(node, seen);
		else
			Error::ThrowError("Config Error: unknown directive '" + directive + "' in location block");
	}
	DDEBUG("Validation") << "    Location block validation passed.";
}

// ──────────────────────────────────────
//  SERVER-SCOPE DIRECTIVE VALIDATORS
// ──────────────────────────────────────

// --- listen ---
// Allowed formats: "8080", "localhost", "0.0.0.0:8080"
// Multiple listen directives are allowed in the same server block.
void Validation::validateListen(AST<string> &node, ServerSeen &seen)
{
	seen.listen = true;
	checkArgCount(node, 1, 1, "listen");

	const string &val = node.GetArguments()[0];
	string port;

	string::size_type pos = val.rfind(':');
	if (pos != string::npos)
    	port = val.substr(pos + 1);
	else
	{
		bool numeric = true;
		for (size_t i = 0; i < val.size(); i++)
			if (!isdigit(val[i]))
			{
				numeric = false;
				break;
			}
		if ( numeric )
			port = val;
		else
			port = "80";
	}

	long portNum = parseNumber(port);
	if (portNum < 1 || portNum > 65535)
		Error::ThrowError("Config Error: listen port out of range (1-65535): " + port);
	DDEBUG("Validation") << "    'listen' validated: " << val;
}

// --- server_name ---
void Validation::validateServerName(AST<string> &node, ServerSeen &seen)
{
	if (seen.serverName)
		Error::ThrowError("Config Error: duplicate 'server_name' in server block");
	seen.serverName = true;
	requireArgs(node, "server_name");
	DDEBUG("Validation") << "    'server_name' validated: " << node.GetArguments()[0];
}

// --- root ---
void Validation::validateRoot(AST<string> &node, bool &seen)
{
	if (seen)
		Error::ThrowError("Config Error: duplicate 'root' directive");
	seen = true;
	checkArgCount(node, 1, 1, "root");

	DDEBUG("Validation") << "    'root' validated: " << node.GetArguments()[0];
}

// --- index ---
void Validation::validateIndex(AST<string> &node, bool &seen)
{
	if (seen)
		Error::ThrowError("Config Error: duplicate 'index' directive");
	seen = true;
	requireArgs(node, "index");

	const vector<string> &args = node.GetArguments();
	for (size_t i = 0; i < args.size(); i++)
	{
		if (args[i].find('/') != string::npos)
			Error::ThrowError("Config Error: index value must not be an absolute path: '" + args[i] + "'");
		DDEBUG("Validation") << "    'index' file [" << i << "] validated: " << args[i];
	}
	DDEBUG("Validation") << "    'index' validated with " << args.size() << " file(s).";
}

// --- autoindex ---
void Validation::validateAutoindex(AST<string> &node, bool &seen)
{
	if (seen)
		Error::ThrowError("Config Error: duplicate 'autoindex' directive");
	seen = true;
	checkArgCount(node, 1, 1, "autoindex");

	const string &val = node.GetArguments()[0];
	if (val != "on" && val != "off")
		Error::ThrowError("Config Error: autoindex value must be 'on' or 'off', got '" + val + "'");
	DDEBUG("Validation") << "    'autoindex' validated: " << val;
}

// --- return ---
void Validation::validateReturn(AST<string> &node, bool &seen)
{
	DDEBUG("Validation") << "ENTER validateReturn(server)";
	if (seen)
		Error::ThrowError("Config Error: duplicate 'return' directive");
	seen = true;
	checkArgCount(node, 1, 2, "return");

	long code = parseNumber(node.GetArguments()[0]);
	if (code != 200 && code != 301 && code != 302)
		Error::ThrowError("Config Error: return code must be 200, 301, or 302 — got '" + node.GetArguments()[0] + "'");
	
	string str;
	if ( node.GetArguments().size() > 1)
		str = ", url=" + node.GetArguments()[1];
	DDEBUG("Validation") << "    'return' validated: code=" << node.GetArguments()[0] << str;
}

void Validation::validateReturn(AST<string> &node, LocationSeen &seen)
{
	DDEBUG("Validation") << "ENTER validateReturn(location)";
	if (seen.returnDir)
		Error::ThrowError("Config Error: duplicate 'return' directive");

	seen.returnDir = true;
	checkArgCount(node, 1, 2, "return");

	long code = parseNumber(node.GetArguments()[0]);
	if (code != 200 && code != 301 && code != 302)
		Error::ThrowError("Config Error: return code must be 200, 301, or 302 — got '" + node.GetArguments()[0] + "'");
	
	if ( seen.bodyInFile && code != 200 )
		Error::ThrowError("Config Error: 'return 301/302' cannot be used with 'client_body_in_file_only' in the same location block");
	if (seen.cgiPass && code != 200)
		Error::ThrowError("Config Error: 'return 301/302' cannot be used with 'cgi_pass'");
	if (seen.deleteFiles && code != 200)
		Error::ThrowError("Config Error: 'return 301/302' cannot be used with 'delete_files'");

	seen.returnKind = classifyReturnCode(code);
	string str;
	if ( node.GetArguments().size() > 1)
		str = ", url=" + node.GetArguments()[1];
	DDEBUG("Validation") << "    'return' validated: code=" << node.GetArguments()[0] << " with returnKind=" << seen.returnKind << str;
}

// --- client_max_body_size ---
void Validation::validateClientMaxBody(AST<string> &node, bool &seen)
{
	if (seen)
		Error::ThrowError("Config Error: duplicate 'client_max_body_size' directive");
	seen = true;
	checkArgCount(node, 1, 1, "client_max_body_size");

	const string &val = node.GetArguments()[0];
	if (val.empty())
		Error::ThrowError("Config Error: client_max_body_size has empty value");

	char unit = val[val.size() - 1];
	if (!isByteSizeUnit(unit))
		Error::ThrowError("Config Error: client_max_body_size must end with a unit (b/k/m/g): '" + val + "'");

	long numericPart = parseNumber(val.substr(0, val.size() - 1));
	DDEBUG("Validation") << "    'client_max_body_size' validated: " << val
						<< " (numeric=" << numericPart << ", unit=" << unit << ")";
}

// --- allow_methods ---
void Validation::validateAllowMethods(AST<string> &node, bool &seen)
{
	if (seen)
		Error::ThrowError("Config Error: duplicate 'allow_methods' directive");
	seen = true;
	requireArgs(node, "allow_methods");

	const vector<string> &args = node.GetArguments();
	for (size_t i = 0; i < args.size(); i++)
	{
		if (!isValidMethod(args[i]))
			Error::ThrowError("Config Error: invalid HTTP method '" + args[i] +
							  "' — allowed: GET, POST, DELETE");
		DDEBUG("Validation") << "    'allow_methods' method [" << i << "] validated: " << args[i];
	}
	DDEBUG("Validation") << "    'allow_methods' validated with " << args.size() << " method(s).";
}

// --- error_page ---
void Validation::validateErrorPage(AST<string> &node, bool &seen)
{
	seen = true;
	checkArgCount(node, 2, 2, "error_page");

	long code = parseNumber(node.GetArguments()[0]);
	bool validCode = (code >= 400 && code <= 404) || (code >= 500 && code <= 504);
	if (!validCode)
		Error::ThrowError("Config Error: error_page code must be 4xx or 5xx — got '" +
						  node.GetArguments()[0] + "'");
	DDEBUG("Validation") << "    'error_page' validated: code=" << node.GetArguments()[0]
						  << ", page=" << node.GetArguments()[1];
}

// ──────────────────────────────────────
//  LOCATION-ONLY DIRECTIVE VALIDATORS
// ──────────────────────────────────────

// --- cgi_pass ---
void Validation::validateCgiPass(AST<string> &node, LocationSeen &seen)
{
	if (seen.cgiPass)
		Error::ThrowError("Config Error: duplicate 'cgi_pass' in location block");
	if (seen.bodyInFile)
		Error::ThrowError("Config Error: 'cgi_pass' cannot be used with 'client_body_in_file_only'");
	if (seen.deleteFiles)
		Error::ThrowError("Config Error: 'cgi_pass' cannot be used with 'delete_files'");
	if ( seen.returnKind == RETURN_REDIRECT )
		Error::ThrowError("Config Error: 'cgi_pass' cannot be used with 'return 301/302' in the same location block");

	seen.cgiPass = true;
	checkArgCount(node, 2, 2, "cgi_pass");

	const string &ext = node.GetArguments()[0];
	if (ext.empty() || ext[0] != '.')
		Error::ThrowError("Config Error: cgi_pass extension must start with a dot — got '" + ext + "'");
	DDEBUG("Validation") << "    'cgi_pass' validated: extension=" << ext
						  << ", path=" << node.GetArguments()[1];
}

// --- client_body_in_file_only ---
void Validation::validateBodyInFile(AST<string> &node, LocationSeen &seen)
{
	if (seen.bodyInFile)
		Error::ThrowError("Config Error: duplicate 'client_body_in_file_only' in location block");
	if (seen.cgiPass)
		Error::ThrowError("Config Error: 'client_body_in_file_only' cannot be used with 'cgi_pass'");
	if ( seen.returnKind == RETURN_REDIRECT )
		Error::ThrowError("Config Error: 'client_body_in_file_only' cannot be used with 'return 301/302' in the same location block");

	seen.bodyInFile = true;
	checkArgCount(node, 1, 1, "client_body_in_file_only");

	const string &val = node.GetArguments()[0];
	if (val != "on" && val != "off")
		Error::ThrowError("Config Error: client_body_in_file_only must be 'on' or 'off' — got '" + val + "'");
	DDEBUG("Validation") << "    'client_body_in_file_only' validated: " << val;
}

// --- delete_files ---
void Validation::validateDeleteFiles(AST<string> &node, LocationSeen &seen)
{
	if (seen.deleteFiles)
		Error::ThrowError("Config Error: duplicate 'delete_files' in location block");
	if (seen.cgiPass)
		Error::ThrowError("Config Error: 'delete_files' cannot be used with 'cgi_pass'");
	if ( seen.returnKind == RETURN_REDIRECT )
		Error::ThrowError("Config Error: 'delete_files' cannot be used with 'return 301/302' in the same location block");

	seen.deleteFiles = true;
	checkArgCount(node, 1, 1, "delete_files");

	const string &val = node.GetArguments()[0];
	if (val != "on" && val != "off")
		Error::ThrowError("Config Error: delete_files must be 'on' or 'off' — got '" + val + "'");
	DDEBUG("Validation") << "    'delete_files' validated: " << val;
}

// ──────────────────────────────────────
//  HELPERS
// ──────────────────────────────────────

ReturnKind Validation::classifyReturnCode(long code)
{
    if (code == 200) 
		return RETURN_200;
	if ( code == 301 || code == 302)
	    return RETURN_REDIRECT;
	return RETURN_NONE;
}

long Validation::parseNumber(const string &s)
{
	if (s.empty())
		Error::ThrowError("Config Error: expected a number, got empty string");

	char *endptr = NULL;
	errno = 0;
	long val = strtol(s.c_str(), &endptr, 10);

	if (errno != 0 || endptr == s.c_str() || *endptr != '\0' || s[0] == '+')
		Error::ThrowError("Config Error: invalid number '" + s + "'");

	return val;
}

bool Validation::isByteSizeUnit(char c)
{
	return (c == 'b' || c == 'B' ||
			c == 'k' || c == 'K' ||
			c == 'm' || c == 'M' ||
			c == 'g' || c == 'G');
}

bool Validation::isValidMethod(const string &m)
{
	return (m == "GET" || m == "POST" || m == "DELETE");
}

static string intToStr(int n)
{
	ostringstream oss;
	oss << n;
	return oss.str();
}

void Validation::checkArgCount(AST<string> &node, size_t min, size_t max, const string &name)
{
	size_t count = node.GetArguments().size();
	if (count < min)
		Error::ThrowError("Config Error: '" + name + "' expects at least " +
						  intToStr((int)min) + " argument(s), got " +
						  intToStr((int)count));
	if (count > max)
		Error::ThrowError("Config Error: '" + name + "' expects at most " +
						  intToStr((int)max) + " argument(s), got " +
						  intToStr((int)count));
}

void Validation::requireArgs(AST<string> &node, const string &name)
{
	if (node.GetArguments().empty())
		Error::ThrowError("Config Error: '" + name + "' requires at least one argument");
}
