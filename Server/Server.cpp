#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <boost/filesystem.hpp>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <locale>
#include <codecvt>
#include <string>
using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace boost::filesystem;

#define TRACE(msg)            std::wcout << msg
#define TRACE_ACTION(a, k, v) std::wcout << a << U(" (") << k << U(", ") << v << U(")\n")

boost::filesystem::path directory_path;
std::map<utility::string_t, utility::string_t> dictionary;

void display_json(
	json::value const & jvalue,
	utility::string_t const & prefix)
{
	std::wcout << prefix << jvalue.serialize() << std::endl;
}

void handle_get(http_request request, path search_path, long int page_size, long int page_index)
{
	int index = 0;
	utility::string_t trace_string = U("\nhandle ") + request.method() + U("\n");
	TRACE(trace_string);
	std::vector<std::string> results;
	std::vector<json::value> paged_results;
	for (directory_iterator it = directory_iterator(search_path); it != directory_iterator(); ++it) { //iterate through directory to get all files and folders
		results.push_back(it->path().filename().string());
	}	
	//TODO implement better sorting method (multithreaded implementation?)
	std::sort(results.begin(), results.end());
	for (std::vector<std::string>::iterator it = results.begin() + page_size * page_index; it != results.end() && index < page_size; ++it) {	//iterate through sorted results to get page
		utility::string_t file_name = utility::conversions::utf8_to_utf16(*it);
		paged_results.push_back(json::value(file_name));
		++index;
	}
	auto answer = json::value::array(paged_results);
	display_json(json::value::null(), U("R: "));
	const method &mtd = request.method();
	if (mtd == methods::GET) {
		display_json(answer, U("S: "));
		request.reply(status_codes::OK, answer);
	}
	else {
		http_response response(status_codes::OK);
		auto body_text = utility::conversions::to_utf8string(answer.serialize());
		auto length = body_text.size();
		response.headers().add(U("Content-Type"), U("application/json"));
		response.headers().add(U("Content-Length"), length);
		display_json(json::value::null(), U("S: "));
		request.reply(response);
	}
}

void routingHandler(http_request request) {
	const method &mtd = request.method();
	utility::string_t trace_string = U("\nhandle ") + mtd + U("\n");
	TRACE(trace_string);

	if (mtd == methods::GET || mtd == methods::HEAD) {
		utility::string_t request_uri_path = request.request_uri().path();
		auto http_get_vars = uri::split_query(web::uri::decode(request.request_uri().query()));
		if (request_uri_path.compare(L"/getDir") == 0 && 
			http_get_vars.size() == 3 && 
			http_get_vars.begin()->first.compare(L"dir") == 0 && 
			(++http_get_vars.begin())->first.compare(L"pageNum") == 0 && 
			(++++http_get_vars.begin())->first.compare(L"pageSize") == 0) {
			path search_path = directory_path;
			long int page_index;
			long int page_size;
			if (http_get_vars.begin()->second.compare(L"\"\"") != 0) {
				search_path.append(http_get_vars.begin()->second);
			}
			wchar_t *pEnd;
			page_index = wcstol((++http_get_vars.begin())->second.c_str(), &pEnd, 10);
			page_size = wcstol((++++http_get_vars.begin())->second.c_str(), &pEnd, 10);
			handle_get(request, search_path, page_size, page_index);
		}
		else {
			request.reply(status_codes::BadRequest);
		}
	}
	
	else {
		http_response response;
		(mtd == methods::OPTIONS) ? response.set_status_code(status_codes::OK) : response.set_status_code(status_codes::MethodNotAllowed);
		response.headers().add(U("Allow"), U("GET,HEAD"));
		request.reply(response);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::wcout << U("Usage: App_Name server_address(eg. http://example.com/fileapp) directory_path(eg. C:\\Server\\FileDirectory)\n");
		return 1;
	}

	try
	{
		path p = path(argv[2]);   // p reads clearer than argv[2] in the following code
		while (!exists(p) || !is_directory(p)) {
			if (!exists(p)) {
				std::wcout << p << L" does not exist.\nPlease input the base file directory.\n";
			}
			else {
				std::wcout << p << L" is not a valid directory.\nPlease input the base file directory.\n";
			}
			std::cin >> p;
		}
	}

	catch (const filesystem_error& ex)
	{
		std::cout << ex.what() << '\n';
		return 1;
	}

	try {
		web::uri my_uri(utility::conversions::utf8_to_utf16(argv[1]));
		directory_path = path(argv[2]);
		http_listener listener(my_uri);
		listener.support(routingHandler);
		listener.support(methods::OPTIONS, routingHandler);
		listener.open().then([&listener]() { TRACE(U("\nstarting to listen\n")); }).wait();
		while (true);
	}

	catch (std::exception const & e) {
		std::wcout << e.what() << std::endl;
		return 1;
	}

	return 0;
}