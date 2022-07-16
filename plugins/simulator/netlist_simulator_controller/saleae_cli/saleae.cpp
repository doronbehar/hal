#include <iostream>
#include <iomanip>
#include <netlist_simulator_controller/saleae_directory.h>
#include <netlist_simulator_controller/saleae_file.h>
#include <hal_core/utilities/program_options.h>
#include <hal_core/utilities/program_arguments.h>
#include <hal_core/utilities/log.h>

using namespace hal;

// checks if a file exists 
bool file_exists (const std::string& path)
{
    if (FILE *ff = fopen(path.c_str(), "rb"))
    {
        fclose(ff);
        return true;
    }
    return false;   
}

// template for space saving printing
template<typename T> void print_element(T t, const int& width, bool align)
{
    if (align)
    {
        std::cout << std::left << std::setw(width) << std::setfill(' ') << t << " | ";
    }
    else
    {
        std::cout << std::right << std::setw(width) << std::setfill(' ') << t << " | ";
    }
}

// check, given the size, whether an entry may be printed
bool check_size(bool necessary, char op, int size_val, int compare_val)
{
    if (!necessary) return true;
    switch (op)
    {
        case '+':
            return (compare_val > size_val);
        case '-':
            return (compare_val < size_val);
        default:
            return (compare_val == size_val);
    }
}

// check, given an id list, whether an entry may be printed
bool check_ids(bool necessary, std::unordered_set<int> id_set, int id_to_check)
{
    return ((id_set.count(id_to_check)) || (!necessary));
}

// saleae ls-tool
void saleae_ls(std::string path, std::string size, std::string ids)
{
    // handle --dir option
    path = (path == "") ? "./saleae.json" : path + "/saleae.json";
    if (!file_exists(path))
    {
        std::cout << "Cannot open file: " << path << std::endl;
        return;
    }

    // handle --size option
    bool size_necessary = false;
    char size_op = '=';
    int size_val = 0;
    if (size != "")
    {
        size_necessary = true;
        if (size[0] == '+' || size[0] == '-')
        {
            size_op = size[0];
            size_val = std::stoi(size.substr(1));
        }
        else
        {
            size_val = std::stoi(size);
        }
    }

    // handle --id option
    bool ids_necessary = false;
    std::unordered_set<int> id_set;
    if (ids != "")
    {
        ids_necessary = true;
        std::stringstream ss(ids);
        std::vector<std::string> splited_ids;
        while (ss.good())
        {
            std::string substr;
            getline(ss, substr, ',');
            splited_ids.push_back(substr);
        }

        for (std::string id_entry : splited_ids)
        {
            if (id_entry.find('-') != std::string::npos)
            {
                std::stringstream range_stream(id_entry);
                std::vector<std::string> range;
                while (range_stream.good())
                {
                    std::string substr;
                    getline(range_stream, substr, '-');
                    range.push_back(substr);
                }
                int tmp_id = std::stoi(range.front());
                while (tmp_id <= std::stoi(range.back()))
                {
                    id_set.insert(tmp_id);
                    tmp_id ++;
                }
            }
            else
            {
               id_set.insert(std::stoi(id_entry));
            }
        }
    }

    SaleaeDirectory *sd = new SaleaeDirectory(path, false);
    std::vector<SaleaeDirectoryNetEntry> net_entries = sd->dump();

    // collect length for better formatting
    int format_length [6] = {7, 8, 19, 11, 10, 15}; // length of the column titles
    for (const SaleaeDirectoryNetEntry& sdne : net_entries)
    {
        for (const SaleaeDirectoryFileIndex& sdfi : sdne.indexes())
        {
            if (check_size(size_necessary, size_op, size_val, sdfi.numberValues()) && check_ids(ids_necessary, id_set, sdne.id()))
            {
                format_length[0] = (format_length[0] < std::to_string(sdne.id()).length()) ? std::to_string(sdne.id()).length() : format_length[0];
                format_length[1] = (format_length[1] < sdne.name().length()) ? sdne.name().length() : format_length[1];
                format_length[2] = (format_length[2] < std::to_string(sdfi.numberValues()).length()) ? std::to_string(sdfi.numberValues()).length() : format_length[2];
                format_length[3] = (format_length[3] < std::to_string(sdfi.beginTime()).length()) ? std::to_string(sdfi.beginTime()).length() : format_length[3];
                format_length[4] = (format_length[4] < std::to_string(sdfi.endTime()).length()) ? std::to_string(sdfi.endTime()).length() : format_length[4];
                format_length[5] = (format_length[5] < std::to_string(sdfi.index()).length() + 12) ? std::to_string(sdfi.index()).length() + 12: format_length[5];
            }
        }
    }
    int abs_length = format_length[0] + format_length[1] + format_length[2] + format_length[3] + format_length[4] + format_length[5] + 16;

    // print saleae-dir content
    std::cout << std::string(abs_length + 2, '-') << std::endl;
    print_element("| Net ID", format_length[0], true);
    print_element("Net Name", format_length[1], true);
    print_element("Total Number Values", format_length[2], true);
    print_element("First Event", format_length[3], true);
    print_element("Last Event", format_length[4], true);
    print_element("Binary filename", format_length[5], true);
    std::cout << std::endl;
    std::cout << '|' << std::string(abs_length, '-') << '|' << std::endl;
    for (const SaleaeDirectoryNetEntry& sdne : net_entries)
    {
        for (const SaleaeDirectoryFileIndex& sdfi : sdne.indexes())
        {
            if (check_size(size_necessary, size_op, size_val, sdfi.numberValues()) && check_ids(ids_necessary, id_set, sdne.id()))
            {
                std::cout << '|';
                print_element(sdne.id(), format_length[0], false);
                print_element(sdne.name(), format_length[1], true);
                print_element(sdfi.numberValues(), format_length[2], false);
                print_element(sdfi.beginTime(), format_length[3], false);
                print_element(sdfi.endTime(), format_length[4], false);
                print_element("digital_" + std::to_string(sdfi.index()) + ".bin", format_length[5], true);
                std::cout << std::endl;
            }
        }
    }
    std::cout << std::string(abs_length + 2, '-') << std::endl;
}


void saleae_cat(std::string path, std::string file_name, bool dump_header, bool dump_data)
{
    // handle --dir option
    path = (path == "") ? "./" + file_name : path + "/" + file_name;
    if (!file_exists(path))
    {
        std::cout << "Cannot open file: " << path << std::endl;
        return;
    }

    // handle -b and -h option
    if (!dump_header && !dump_data)
    {
        dump_header = true;
        dump_data = true;
    }

    SaleaeInputFile *sf = new SaleaeInputFile(path);
    uint64_t num_transitions = sf->header()->mNumTransitions;

    // dump header
    if (dump_header)
    {
        // get header content
        uint64_t begin_time = sf->header()->mBeginTime;
        uint64_t end_time = sf->header()->mEndTime;
        std::string data_format;
        switch (sf->header()->storageFormat())
        {
        case SaleaeHeader::Double:
            data_format = "Double";
            break;
        case SaleaeHeader::Uint64:
            data_format = "Uint64";
            break;
        case SaleaeHeader::Coded:
            data_format = "Coded";
            break;
        }

        // collect length for better formatting
        int format_length [4] = {12, 11, 9, 21}; // length of the column titles
        format_length[0] = (format_length[0] < data_format.length()) ? data_format.length() : format_length[0];
        format_length[1] = (format_length[1] < std::to_string(begin_time).length()) ? std::to_string(begin_time).length() : format_length[1];
        format_length[2] = (format_length[2] < std::to_string(end_time).length()) ? std::to_string(end_time).length() : format_length[2];
        format_length[3] = (format_length[3] < std::to_string(num_transitions).length()) ? std::to_string(num_transitions).length() : format_length[3];
        int abs_length = format_length[0] + format_length[1] + format_length[2] + format_length[3] + 10;

        // print saleae-file header
        std::cout << std::string(abs_length + 2, '-') << std::endl;
        std::cout << '|';
        print_element(" Data Format", format_length[0], true);
        print_element("Start Value", format_length[1], true);
        print_element("End Value", format_length[2], true);
        print_element("Number of Transitions", format_length[3], true);
        std::cout << std::endl;
        std::cout << '|' << std::string(abs_length, '-') << '|' << std::endl;
        std::cout << "| ";
        print_element(data_format, format_length[0] - 1, true);
        print_element(begin_time, format_length[1], false);
        print_element(end_time, format_length[2], false);
        print_element(num_transitions, format_length[3], false);
        std::cout << std::endl;
        std::cout << std::string(abs_length + 2, '-') << std::endl;
    }

    // dump data
    if (dump_data)
    {
        SaleaeDataBuffer *db = sf->get_buffered_data(num_transitions);

        // get data
        uint64_t* time_array = db->mTimeArray;
        int* value_array = db->mValueArray;

        // collect length for better formatting
        int format_length [3] = {4, 4, 5}; // length of the column titles
        for (int i = 0; i < num_transitions; i++)
        {
            format_length[0] = (format_length[0] < std::to_string(i).length()) ? std::to_string(i).length() : format_length[0];
            format_length[1] = (format_length[1] < std::to_string(time_array[i]).length()) ? std::to_string(time_array[i]).length() : format_length[1];
            format_length[2] = (format_length[2] < std::to_string(value_array[i]).length()) ? std::to_string(time_array[i]).length() : format_length[2];
        }
        int abs_length = format_length[0] + format_length[1] + format_length[2] + 7;

        // print saleae-file data
        std::cout << std::string(abs_length + 2, '-') << std::endl;
        std::cout << '|';
        print_element(" No.", format_length[0], true);
        print_element("Time", format_length[1], true);
        print_element("Value", format_length[2], true);
        std::cout << std::endl;
        std::cout << '|' << std::string(abs_length, '-') << '|' << std::endl;
        for (int i = 0; i < num_transitions; i++)
        {
            std::cout << '|';
            print_element(i, format_length[0], false);
            print_element(time_array[i], format_length[1], false);
            print_element(value_array[i], format_length[2], false);
            std::cout << std::endl;
        }
        std::cout << std::string(abs_length + 2, '-') << std::endl;
    }
}


int main(int argc, const char* argv[])
{

    // initialize logging
    LogManager& lm = LogManager::get_instance();
    lm.add_channel("core", {LogManager::create_stdout_sink(), LogManager::create_file_sink(), LogManager::create_gui_sink()}, "info");
    lm.deactivate_all_channels();

    // initialize and parse options
    ProgramOptions generic_options("generic options");
    generic_options.add("--help", "print help messages");

    ProgramOptions tool_options("tool options");
    tool_options.add("ls", "Lists content of saleae directory file saleae.json");
    tool_options.add("cat", "Dump content of binary file into console", {""});

    ProgramOptions ls_options("ls options");
    ls_options.add({"-d", "--dir"}, "lists saleae directory from directory given by absolute or relative path name", {ProgramOptions::A_REQUIRED_PARAMETER});
    ls_options.add({"-s", "--size"}, "lists only entries with given number of waveform events", {ProgramOptions::A_REQUIRED_PARAMETER});
    ls_options.add({"-i", "--id"}, "list only entries where ID matches entry in list. Entries are separated by comma. A single entry can be either an ID or a range sepearated by hyphen", {ProgramOptions::A_REQUIRED_PARAMETER});

    ProgramOptions cat_options("cat options");
    cat_options.add({"-d", "--dir"}, "binary file is not in current directory but in directory given by path name", {ProgramOptions::A_REQUIRED_PARAMETER});
    cat_options.add({"-h", "--only-header"}, "dump only header");
    cat_options.add({"-b", "--only-data"}, "dump only data including start value");

    ProgramArguments args = tool_options.parse(argc, argv);

    if (args.is_option_set("ls"))
    {
        ls_options.add(generic_options);
        ProgramArguments args = ls_options.parse(argc, argv);

        bool unknown_option_exists = false;
        for (std::string opt : ls_options.get_unknown_arguments())
        {
            unknown_option_exists = (opt != "ls") ? true : unknown_option_exists;     
        }

        if (args.is_option_set("--help") || unknown_option_exists)
        {
            std::cout << ls_options.get_options_string() << std::endl;
        } else
        {
            saleae_ls(args.get_parameter("--dir"), args.get_parameter("--size"), args.get_parameter("--id"));
        }
    } else if (args.is_option_set("cat"))
    {
        std::string filename = args.get_parameter("cat");
        cat_options.add(generic_options);
        ProgramArguments args = cat_options.parse(argc, argv);

        bool unknown_option_exists = false;
        for (std::string opt : cat_options.get_unknown_arguments())
        {
            unknown_option_exists = ((opt != "cat") && (opt != filename)) ? true : unknown_option_exists;
        }
        
        if (filename == "")
        {
            std::cout << tool_options.get_options_string();
            std::cout << ls_options.get_options_string();
            std::cout << cat_options.get_options_string() << std::endl;
        }
        else if (args.is_option_set("--help") || unknown_option_exists)
        {
            std::cout << cat_options.get_options_string() << std::endl;
        } else
        {
            saleae_cat(args.get_parameter("--dir"), filename, args.is_option_set("--only-header"), args.is_option_set("--only-data"));
        }
    } else
    {
        tool_options.add(generic_options);

        std::cout << tool_options.get_options_string();
        std::cout << ls_options.get_options_string();
        std::cout << cat_options.get_options_string() << std::endl;
    }
}
