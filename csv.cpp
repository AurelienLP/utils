#include "csv.h"
#include "logger.h"
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>

typedef boost::tokenizer<boost::escaped_list_separator<char> > Tokenizer;

CsvReader::CsvReader(const std::string& filename, char separator, bool read_headers, std::string encoding): filename(filename),
    file(), closed(false), functor('\\', separator, '"') 
#ifdef HAVE_ICONV_H
	, converter(NULL)
#endif
{
    file.open(filename);
    stream = new std::istream(file.rdbuf());
    stream->setstate(file.rdstate());
    if(encoding != "UTF-8"){
        //TODO la taille en dur s'mal
#ifdef HAVE_ICONV_H
        converter = new EncodingConverter(encoding, "UTF-8", 2048);
#endif
    }

    if(file.is_open()) {

        remove_bom(file);

        if(read_headers) {
            auto line = next();
            for(size_t i=0; i<line.size(); ++i)
                this->headers.insert(std::make_pair(line[i], i));
        }
    } else {
        closed = true;
    }
}

CsvReader::CsvReader(std::stringstream &sstream, char separator, bool read_headers, std::string encoding): filename("sstream"),
    file(), closed(false), functor('\\', separator, '"') 
#ifdef HAVE_ICONV_H
	, converter(NULL)
#endif
{
    stream = new std::istream(sstream.rdbuf());
    if(encoding != "UTF-8"){
        //TODO la taille en dur s'mal
#ifdef HAVE_ICONV_H
        converter = new EncodingConverter(encoding, "UTF-8", 2048);
#endif
    }

    if(read_headers) {
        auto line = next();
        for(size_t i=0; i<line.size(); ++i)
            this->headers.insert(std::make_pair(line[i], i));
    } 
}

bool CsvReader::is_open() {
    return stream->good();
}

bool CsvReader::validate(const std::vector<std::string> &mandatory_headers) {
    for(auto header : mandatory_headers)
        if(headers.find(header) == headers.end())
            return false;
    return true;
}

std::string CsvReader::missing_headers(const std::vector<std::string> &mandatory_headers) {
    std::string result;
    for(auto header : mandatory_headers)
        if(headers.find(header) == headers.end())
            result += header + ", ";

    return result;

}

void CsvReader::close(){
    if(!closed){
        file.close();
#ifdef HAVE_ICONV_H
		//TODO g�rer des options de compile plutot que par plateforme
        if(converter != NULL) {
            delete converter;
            converter = NULL;
        }
#endif
        closed = true;
    }
}

bool CsvReader::eof() const{
    return stream->eof() | stream->bad() | stream->fail();
}

CsvReader::~CsvReader(){
    this->close();
}

std::vector<std::string> CsvReader::next(){
    if(!is_open()){
        throw std::exception();
    }

    do{
        if(eof()){
            return std::vector<std::string>();
        }
        std::getline(*stream, line);
    }while(line.empty());


#ifdef HAVE_ICONV_H
    if(converter != NULL){
        line = converter->convert(line);
    }
#endif

    boost::trim(line);
    std::vector<std::string> vec;
    try {
        Tokenizer tok(line, functor);
        vec.assign(tok.begin(), tok.end());
        for(auto &s: vec)
            boost::trim(s);
    } catch(...) {
        LOG4CPLUS_WARN(log4cplus::Logger::getInstance("log") ,"Impossible de parser la ligne :  " + line);
        return next();
    }

    return vec;
}

int CsvReader::get_pos_col(const std::string & str){
    auto it = headers.find(str);  /// Utilisation dans le cas o� le key n'existe pas size_t = 0

    if (it != headers.end())
        return headers[str];
    return -1;
}

void remove_bom(std::fstream& stream){
    char buffer[3];
    stream.read(buffer, 3);
    if(stream.gcount() != 3)
        return;
    if(buffer[0] == (char)0xEF && buffer[1] == (char)0xBB){
        //BOM UTF8
        return;
    }
    
    //pas de correspondance avec un BOM, on remet les caract�res lus
    for(int i=0; i<3; i++){
        stream.unget();
    }
}
