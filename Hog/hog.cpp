

int hog(std::string filename){

    exec(filename);



    return 0;
}

int exec(std::string filename){
    code = read(filename);
    compile(code);
    run(code);
}

