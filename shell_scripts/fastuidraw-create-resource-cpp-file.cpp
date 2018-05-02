#include <iostream>
#include <fstream>
#include <string>

using namespace std;

void show_usage(const char* app)
{
    cerr << "Usage: " << app << " input_file output_name output_directory" << endl
        << "Creates a .cpp file named output_name.cpp in the directory output_directory" << endl
        << "which when added to a project adds a resource for fastuidraw::fetch_static_resource()" << endl
        << "named output_name with value the contents of input_file. " << endl;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        show_usage(argv[0]);
        return EXIT_FAILURE;
    }

    string in_filename(argv[1]);
    string out_filename(argv[2]);
    string out_dirname(argv[3]);

    ifstream inf(in_filename);
    if (inf.fail()) {
        cerr << "Input file " << in_filename << " not found" << endl;
        return EXIT_FAILURE;
    }

    string out_path = out_dirname + '/' + out_filename + ".cpp";
    ofstream outf(out_path, ios::binary | ios::out);
    if (outf.fail()) {
        cerr << "Can't open output file at:" << out_path << endl;
        return EXIT_FAILURE;
    }

    outf << "#include <fastuidraw/util/static_resource.hpp>\n" << endl
         << "namespace { \n\tconst uint8_t values[]={ " << endl;

    string line;
    while (getline(inf, line).good()) {
        for (int c : line) {
            outf << c << ","; 
        }
        outf << 10 << ","; // End-of-Line
    }

    outf << " 0 };" << endl
         <<" fastuidraw::static_resource R(\"" << out_filename << "\", fastuidraw::c_array<const uint8_t>(values, sizeof(values)));" << endl
         <<"\n}" << endl;


    return EXIT_SUCCESS;
}
