#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

namespace hal
{
    class SaleaeDirectoryFileIndex
    {
        int      mIndex;
        uint64_t mBeginTime;
        uint64_t mEndTime;
        uint64_t mNumberValues;
    public:
        SaleaeDirectoryFileIndex(int inx, uint64_t tBeg=0, uint64_t tEnd=0, uint64_t nval=0)
            : mIndex(inx), mBeginTime(tBeg), mEndTime(tEnd), mNumberValues(nval) {;}
        int index() const { return mIndex; }
        uint64_t beginTime() const { return mBeginTime; }
        uint64_t endTime() const { return mEndTime; }
        uint64_t numberValues() const { return mNumberValues; }
    };

    class SaleaeDirectoryNetEntry
    {
        friend class SaleaeDirectory;
        uint32_t   mId;
        std::string mName;
        std::vector<SaleaeDirectoryFileIndex> mFileIndexes;
    public:
        SaleaeDirectoryNetEntry(const std::string nam, uint32_t id_=0)
            : mId(id_), mName(nam) {;}
        uint32_t id() const { return mId; }
        std::string name() const { return mName; }
        const std::vector<SaleaeDirectoryFileIndex>& indexes() const { return mFileIndexes; }
        void addIndex(const SaleaeDirectoryFileIndex& sdfe) { mFileIndexes.push_back(sdfe); }
        std::string dataFilename() const; // TODO : time as argument
        int dataFileIndex() const; // TODO : time as argument
    };

    class SaleaeDirectory
    {
    public:
        struct ListEntry
        {
            uint32_t id;
            std::string name;
            uint64_t size;
        };
    private:
        std::filesystem::path mDirectoryFile;
        std::vector<SaleaeDirectoryNetEntry> mNetEntries;

        std::unordered_map<uint32_t,int> mById;
        std::unordered_map<std::string, int> mByName;

        std::filesystem::path dataFilename(const SaleaeDirectoryNetEntry& sdnep) const;
        int getIndex(const SaleaeDirectoryNetEntry& sdnep) const;
        int mNextAvailableIndex;
    public:
        SaleaeDirectory(const std::filesystem::path& path, bool create=false);
        bool parse_json();
        bool write_json() const;
        void add_net(SaleaeDirectoryNetEntry& sdne);
        void dump() const;
        void update_file_indexes(std::vector<SaleaeDirectoryFileIndex>& fileIndexes);

        std::filesystem::path get_datafile(const std::string& nam, uint32_t id) const;
        int get_datafile_index(const std::string& nam, uint32_t id) const;

        int get_next_available_index() const { return mNextAvailableIndex; }
        uint64_t get_max_time() const;
        std::filesystem::path get_directory() const { return mDirectoryFile.parent_path(); }

        std::vector<ListEntry> get_net_list() const;
    };
}
