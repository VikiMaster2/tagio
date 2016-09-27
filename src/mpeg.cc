#include "mpeg.h"
#include "configuration.h"
#include "audioproperties.h"
#include "tag.h"
#include "apetag.h"
#include "id3v1tag.h"
#include "id3v2tag.h"

#include "taglib/mpegfile.h"
#include "taglib/id3v1tag.h"
#include "taglib/id3v2tag.h"

using std::string;
using v8::Function;
using v8::Local;
using v8::Value;
using v8::String;
using v8::Object;
using v8::Array;
using Nan::AsyncQueueWorker;
using Nan::AsyncWorker;
using Nan::Callback;
using Nan::HandleScope;
using Nan::New;
using Nan::Null;
using Nan::To;

class ReadMPEGWorker : public AsyncWorker {
public:
    ReadMPEGWorker(Callback *callback, string *path, Configuration *conf)
            : AsyncWorker(callback), path(path), conf(conf) {}
    ~ReadMPEGWorker() {
        delete path;
        delete conf;
        delete file;
    }

    void Execute () {
        file = new TagLib::MPEG::File(path->c_str());
        audioProperties = file->audioProperties();
        tag = file->tag();
        id3v1Tag = file->ID3v1Tag(false);
        id3v2Tag = file->ID3v2Tag(false);
    }

    void HandleOKCallback () {
        HandleScope scope;
        Local<Object> result= New<Object>();

        Local<String> pathKey = New<String>("path").ToLocalChecked();
        Local<String> pathVal = New<String>(path->c_str()).ToLocalChecked();
        result->Set(pathKey, pathVal);

        Local<String> confKey = New<String>("configuration").ToLocalChecked();
        Local<Object> confVal = New<Object>();
        ExportConfiguration(conf, *confVal);
        result->Set(confKey, confVal);

        Local<String> audioPropertiesKey = New<String>("audioProperties").ToLocalChecked();
        Local<Object> audioPropertiesVal = New<Object>();
        ExportAudioProperties(audioProperties, *audioPropertiesVal);
        result->Set(audioPropertiesKey, audioPropertiesVal);

        Local<String> tagKey = New<String>("tag").ToLocalChecked();
        Local<Object> tagVal = New<Object>();
        ExportTag(tag, *tagVal);
        result->Set(tagKey, tagVal);

        if (id3v1Tag != nullptr) {
            Local<String> id3v1Key = New<String>("id3v1").ToLocalChecked();
            Local<Object> id3v1Val = New<Object>();
            ExportID3v1Tag(id3v1Tag, *id3v1Val);
            result->Set(id3v1Key, id3v1Val);
        }

        if (id3v2Tag != nullptr) {
            Local<String> id3v2Key = New<String>("id3v2").ToLocalChecked();
            Local<Array> id3v2Val = New<Array>(id3v2Tag->frameList().size());
            ExportID3v2Tag(id3v2Tag, *id3v2Val, conf);
            result->Set(id3v2Key, id3v2Val);
        }

        Local<Value> argv[] = { Null(), result };
        callback->Call(2, argv);
    }

private:
    string *path;
    Configuration *conf;
    TagLib::MPEG::File *file;

    // extracted
    TagLib::MPEG::Properties *audioProperties;
    TagLib::Tag *tag;
    TagLib::ID3v1::Tag *id3v1Tag;
    TagLib::ID3v2::Tag *id3v2Tag;
};

NAN_METHOD(ReadMPEG) {
    Local<Object> reqObj = info[0].As<Object>();
    Callback *callback = new Callback(info[1].As<Function>());

    Local<String> pathKey = New<String>("path").ToLocalChecked();
    Local<String> pathObj = reqObj->Get(pathKey).As<String>();
    Local<String> confKey = New<String>("configuration").ToLocalChecked();
    Local<Object> confObj = reqObj->Get(confKey).As<Object>();

    String::Utf8Value pathVal(pathObj);
    std::string *path = new std::string(*pathVal);
    Configuration *conf = new Configuration();
    ExportConfiguration(conf, *confObj);

    AsyncQueueWorker(new ReadMPEGWorker(callback, path, conf));
}
