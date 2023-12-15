# SARC Reader/Writer

A  C++ library to read and write SARC files.
## Usage
First of all, include the `SARC.h` header file. To load a SARC, you can pass a path to a file or a vector of unsigned chars, which contains the file data directly. This is especially usefull when loading a with ZStandard compressed BYML.
```cpp
SarcFile SARC("DgnObj_Small_CandlePoleC_04.pack");
```
To write the SARC, you use `WriteToFile`, which takes a path as parameter. You can use `ToBinary`, to write the SARC into a vector of unsigned chars. This is usefull when working with compression.
```cpp
SarcFile SARC("DgnObj_Small_CandlePoleC_04.pack");
SARC.WriteToFile("DgnObj_Small_CandlePoleC_04.pack");
std::vector<unsigned char> Bytes = SARC.ToBinary();
```
To read a file inside the SARC, use `GetEntry`. This function takes a path as a string to the file. It returns a SarcFile::Entry object. The struct contains the file name as a string and the file contents as a vector of unsigned chars. To write the file, call the function `WriteToFile`.
```cpp
SarcFile SARC("DgnObj_Small_CandlePoleC_04.pack");
std::string FileName = SARC.GetEntry("Phive/ShapeParam/DgnObj_Small_CandlePoleC_04__Physical.phive__ShapeParam.bgyml").Name;
std::vector<unsigned char> Bytes = SARC.GetEntry("Phive/ShapeParam/DgnObj_Small_CandlePoleC_04__Physical.phive__ShapeParam.bgyml").Bytes;
SARC.GetEntry("Phive/ShapeParam/DgnObj_Small_CandlePoleC_04__Physical.phive__ShapeParam.bgyml").WriteToFile("C:/x/y/z/DgnObj_Small_CandlePoleC_04__Physical.phive__ShapeParam.bgyml");
```
To modify an entry, use the following methods:
```cpp
SarcFile SARC("DgnObj_Small_CandlePoleC_04.pack");
SARC.GetEntry("Phive/ShapeParam/DgnObj_Small_CandlePoleC_04__Physical.phive__ShapeParam.bgyml").Bytes = NewBytes;
SARC.GetEntry("Phive/ShapeParam/DgnObj_Small_CandlePoleC_04__Physical.phive__ShapeParam.bgyml").Name = "Phive/ShapeParam/Test.phive__ShapeParam.bgyml");
```
To create a new file, create a new SarcFile::Entry object, set the name and bytes and it to the archive.
```cpp
SarcFile SARC("DgnObj_Small_CandlePoleC_04.pack");
SarcFile::Entry File;
File.Name = "Path/to/file/Test.byml";
File.Bytes = Bytes;
SARC.GetEntries().push_back(SARC);
```
