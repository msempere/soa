/* cloud_output.cc
   Jeremy Barnes, 18 September 2012
   Copyright (c) 2012 Datacratic Inc.  All rights reserved.

*/

#include "cloud_output.h"
#include <memory>
#include <boost/filesystem.hpp>
#include "jml/utils/file_functions.h"

namespace Datacratic {
using namespace std;
    using namespace ML;
namespace fs = boost::filesystem;

CloudSink::
CloudSink(const std::string & uri, bool append, bool disambiguate,
          std::string backupDir):
    currentUri_(uri),backupDir_(backupDir)
{
    if (uri != "")
        open(uri, append, disambiguate);
}

CloudSink::
~CloudSink()
{
    //cerr << "CloudSink::~CloudSink::was called with uri " << currentUri_ << endl;
    close();
}

void
CloudSink::
open(const std::string & uri, bool append, bool disambiguate)
{

    cloudStream.close();
    cloudStream.open(uri, std::ios_base::out |
                          (append ? std::ios_base::app : std::ios::openmode()));

    // Get the file name from the s3 uri. We want to preserve the path since
    // if we only get the filename we could overwrite files with the same name
    // but in a different directory. uri format is s3://
    fs::path filePath(backupDir_ + uri.substr(5));
    cerr << "The uri is " << uri.substr(5) << endl;
    // Get the path and create the directories
    fs::create_directories(filePath.parent_path());
    // create the local file and directory
    fileStream.open(filePath.string(), std::ios_base::out |
            (append ? std::ios_base::app : std::ios::openmode()));
}

void
CloudSink::
close()
{
   cloudStream.close();
   fileStream.close();
   fs::path filePath(backupDir_ + currentUri_.substr(5));
   cerr << "Erasing local file " << filePath.string() << endl;
   fs::remove(filePath);
}

size_t
CloudSink::
write(const char * data, size_t size)
{
  //    cerr << "CloudSink::write was called " << endl;
    fileStream.write(data, size);
    cloudStream.write(data, size);
    return size ;
}

size_t
CloudSink::
flush(FileFlushLevel flushLevel)
{
    return 0;
}

void
CloudOutput::uploadIncompleteBackups()
{
    cerr << "About to upload incomplete files in backup directory " << backupDir_ << endl;
    auto onFile = [&] (std::string dir,
                       std::string basename,
                       const struct stat & stats,
                       FileType type,
                       int depth) -> ML::FileAction
    {
        if(type == FT_FILE)
        {
            cerr << "scanning file " << basename << " in directory " << dir << endl;
        }
        return FA_CONTINUE;
    };
    if(fs::exists(backupDir_))
        ML::scanFiles(backupDir_, onFile);
    else
        cerr << "backup directory does not exist" << endl;
}
std::shared_ptr<CompressingOutput::Sink>
CloudOutput::createSink(const string & uri, bool append)
{
    cerr << "CloudOutput::createSink was called with uri " << uri << endl;
    uploadIncompleteBackups();
    return make_shared<CloudSink>(uri, append, true, backupDir_);
}

    RotatingCloudOutput::RotatingCloudOutput(std::string backupDir)
: RotatingOutputAdaptor(std::bind(&RotatingCloudOutput::createFile,
                                      this,
                                  std::placeholders::_1)),
  backupDir_(backupDir)

{

}


void
RotatingCloudOutput::
open(const std::string & filenamePattern,
     const std::string & periodPattern,
     const std::string & compression,
     int level)
{
    this->compression = compression;
    this->level = level;

    
    RotatingOutputAdaptor::open(filenamePattern, periodPattern);
}

RotatingCloudOutput::
~RotatingCloudOutput()
{
    close();
}

CloudOutput *
RotatingCloudOutput::
createFile(const string & filename)
{
    //cerr << "RotatingCloudOutput::createFile. Entering..." << endl;
    std::unique_ptr<CloudOutput> result(new CloudOutput(backupDir_));

    result->onPreFileOpen = [=] (const string & fn)
    {
        if (this->onPreFileOpen)
        {
            this->onPreFileOpen(fn);
        }
    };
    result->onPostFileOpen = [=] (const string & fn)
        { if (this->onPostFileOpen) this->onPostFileOpen(fn); };
    result->onPreFileClose = [=] (const string & fn)
        { if (this->onPreFileClose) this->onPreFileClose(fn); };
    result->onPostFileClose = [=] (const string & fn)
        { if (this->onPostFileClose) this->onPostFileClose(fn); };
    result->onFileWrite = [=] (const string& channel, const std::size_t bytes)
    { if (this->onFileWrite) this->onFileWrite(channel, bytes); };

    result->open(filename, compression, level);
    return result.release();
}


/*****************************************************************************/
/* CLOUD OUTPUT                                                              */
/*****************************************************************************/

CloudOutput::
CloudOutput(std::string backupDir, size_t ringBufferSize)
    : NamedOutput(ringBufferSize),backupDir_(backupDir)
{
}

CloudOutput::
~CloudOutput()
{
}

} // namespace Datacratic
