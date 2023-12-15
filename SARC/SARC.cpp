#include "SARC.h"

void SarcFile::Entry::WriteToFile(std::string Path)
{
    std::ofstream File(Path, std::ios::binary);
    std::copy(this->Bytes.begin(), this->Bytes.end(),
        std::ostream_iterator<unsigned char>(File));
    File.close();
}

std::vector<SarcFile::Entry>& SarcFile::GetEntries()
{
    return this->m_Entries;
}

SarcFile::Entry& SarcFile::GetEntry(std::string Name)
{
    for (SarcFile::Entry& Entry : this->GetEntries())
    {
        if (Entry.Name == Name)
        {
            return Entry;
        }
    }
    return this->GetEntries()[0];
}

SarcFile::SarcFile(std::vector<unsigned char> Bytes)
{
    BinaryVectorReader Reader(Bytes);
    char Magic[4]; //Magic, should be SARC
    Reader.Read(Magic, 4);
    if (Magic[0] != 'S' || Magic[1] != 'A' || Magic[2] != 'R' || Magic[3] != 'C')
    {
        std::cerr << "Expected SARC as magic, but got " << Magic << "!\n";
        return;
    }

    Reader.Seek(4, BinaryVectorReader::Position::Current); //Doesn't matter

    uint32_t FileSize = Reader.ReadUInt32();
    uint32_t DataOffset = Reader.ReadUInt32();

    Reader.Seek(10, BinaryVectorReader::Position::Current); //Padding

    uint16_t Count = Reader.ReadUInt16();

    Reader.Seek(4, BinaryVectorReader::Position::Current); //Padding

    std::vector<SarcFile::Node> Nodes(Count);
    for (int i = 0; i < Count; i++)
    {
        SarcFile::Node Node;
        Node.Hash = Reader.ReadUInt32();
        
        int Attributes = Reader.ReadInt32();

        Node.DataStart = Reader.ReadUInt32();
        Node.DataEnd = Reader.ReadUInt32();
        Node.StringOffset = (Attributes & 0xFFFF) * 4;

        Nodes[i] = Node;
    }

    Reader.Seek(8, BinaryVectorReader::Position::Current); //Padding

    this->m_Entries.resize(Count);

    std::vector<char> StringTableBuffer(DataOffset - Reader.GetPosition()); //This vector will hold the names of the Files inside SARC
    Reader.Read(reinterpret_cast<char*>(StringTableBuffer.data()), DataOffset - Reader.GetPosition()); //Reading String Data into StringTableBuffer

    for (int i = 0; i < Count; i++)
    {
        Reader.Seek(DataOffset + Nodes[i].DataStart, BinaryVectorReader::Position::Begin);
        std::string Name;
        if (i == (Count - 1))
        {
            for (uint32_t j = Nodes[i].StringOffset; j < StringTableBuffer.size() - 1; j++)
            {
                if (std::isprint(StringTableBuffer[j]))
                {
                    Name += StringTableBuffer[j];
                }
                else
                {
                    break;
                }
            }
            
            SarcFile::Entry Entry;
            Entry.Bytes.resize(Nodes[i].DataEnd - Nodes[i].DataStart);
            Reader.Read(reinterpret_cast<char*>(Entry.Bytes.data()), Nodes[i].DataEnd - Nodes[i].DataStart);
            Entry.Name = Name;
            this->m_Entries[i] = Entry;
        }
        else
        {
            for (uint32_t j = Nodes[i].StringOffset; j < Nodes[i + 1].StringOffset - 1; j++)
            {
                Name += StringTableBuffer[j];
            }
            
            SarcFile::Entry Entry;
            Entry.Bytes.resize(Nodes[i].DataEnd - Nodes[i].DataStart);
            Reader.Read(reinterpret_cast<char*>(Entry.Bytes.data()), Nodes[i].DataEnd - Nodes[i].DataStart);
            Entry.Name = Name;
            this->m_Entries[i] = Entry;
        }

        std::cout << this->m_Entries[i].Name << std::endl;
    }
}

int SarcFile::GCD(int a, int b)
{
    while (a != 0 && b != 0)
    {
        if (a > b)
            a %= b;
        else
            b %= a;
    }
    return a | b;
}

int SarcFile::LCM(int a, int b)
{
    return a / GCD(a, b) * b;
}

int SarcFile::GetBinaryFileAlignment(std::vector<unsigned char> Data)
{
    if (Data.size() <= 0x20)
    {
        return 1;
    }

    int32_t FileSize = *reinterpret_cast<const int32_t*>(&Data[0x1C]);
    if (FileSize != static_cast<int32_t>(Data.size()))
    {
        return 1;
    }

    return 1 << Data[0x0E];
}

int SarcFile::AlignUp(int Value, int Size)
{
    return Value + (Size - Value % Size) % Size;
}

std::vector<unsigned char> SarcFile::ToBinary()
{
    BinaryVectorWriter Writer;

    Writer.Seek(0x14, BinaryVectorWriter::Position::Begin);

    uint64_t Count = this->m_Entries.size();

    Writer.WriteBytes("SFAT");
    Writer.WriteInteger(0x0C, sizeof(uint16_t));
    Writer.WriteInteger(Count, sizeof(uint16_t));
    Writer.WriteInteger(0x65, sizeof(uint32_t));

    std::vector<SarcFile::HashValue> Keys(Count);
    for (int i = 0; i < Keys.size(); i++)
    {
        SarcFile::HashValue HashValue;
        HashValue.Node = &this->m_Entries[i];
        int j = 0;
        while (true)
        {
            char c = this->m_Entries[i].Name[j++];
            if (!c)
                break;
            HashValue.Hash = HashValue.Hash * 0x65 + c;
        }
        Keys[i] = HashValue;
    }
    std::sort(Keys.begin(), Keys.end(), [](const SarcFile::HashValue& a, const SarcFile::HashValue& b) { return a.Hash < b.Hash; });

    int RelStringOffset = 0;
    int RelDataOffset = 0;
    int FileAlignment = 1;
    std::vector<uint32_t> Alignments(Count);

    for (int i = 0; i < Count; i++)
    {
        std::string FileName = Keys[i].Node->Name;
        std::vector<unsigned char> Buffer(Keys[i].Node->Bytes.begin(), Keys[i].Node->Bytes.end());
        int Alignment = LCM(4, GetBinaryFileAlignment(Buffer));
        Alignments[i] = Alignment;

        int DataStart = this->AlignUp(RelDataOffset, Alignment);
        int DataEnd = DataStart + Buffer.size();

        Writer.WriteInteger(Keys[i].Hash, sizeof(uint32_t));
        Writer.WriteInteger((0x01000000 | (RelStringOffset / 4)), sizeof(int32_t));
        Writer.WriteInteger(DataStart, sizeof(uint32_t));
        Writer.WriteInteger(DataEnd, sizeof(uint32_t));

        RelDataOffset = DataEnd;
        RelStringOffset += (FileName.length() + 4) & -4;
        FileAlignment = LCM(FileAlignment, Alignment);
    }

    Writer.WriteBytes("SFNT");
    Writer.WriteInteger(0x08, sizeof(uint16_t));
    Writer.WriteInteger(0x00, sizeof(uint16_t));

    for (int i = 0; i < Count; i++)
    {
        std::string FileName = Keys[i].Node->Name;
        std::vector<char> Buffer(FileName.begin(), FileName.end());

        std::vector<char> Aligned(Buffer.size() + 4 & -4);
        for (int j = 0; j < Buffer.size(); j++)
        {
            Aligned[j] = Buffer[j];
        }
        Writer.WriteBytes(Aligned.data());
        Writer.WriteByte(0x00);
    }

    Writer.Seek((FileAlignment - Writer.GetPosition() % FileAlignment) % FileAlignment, BinaryVectorWriter::Position::Current);

    uint32_t DataOffset = Writer.GetPosition();

    for (int i = 0; i < Count; i++)
    {
        Writer.Seek((Alignments[i] - Writer.GetPosition() % Alignments[i]) % Alignments[i], BinaryVectorWriter::Position::Current);
        for (unsigned char Byte : this->m_Entries[i].Bytes)
        {
            Writer.WriteByte(Byte);
        }
    }

    uint32_t FileSize = Writer.GetPosition();

    Writer.Seek(0, BinaryVectorWriter::Position::Begin);

    Writer.WriteBytes("SARC");
    Writer.WriteByte(0x14);
    Writer.WriteByte(0x00);
    Writer.WriteByte(0xFF);
    Writer.WriteByte(0xFE);

    Writer.WriteInteger(FileSize, sizeof(uint32_t));
    Writer.WriteInteger(DataOffset, sizeof(uint32_t));
    Writer.WriteInteger(0x100, sizeof(uint16_t));

    return Writer.GetData();
}

void SarcFile::WriteToFile(std::string Path)
{
    std::ofstream File(Path, std::ios::binary);
    std::vector<unsigned char> Binary = this->ToBinary();
    std::copy(Binary.begin(), Binary.end(),
        std::ostream_iterator<unsigned char>(File));
    File.close();
}

SarcFile::SarcFile(std::string Path)
{
    std::ifstream File(Path, std::ios::binary);

    if (!File.eof() && !File.fail())
    {
        File.seekg(0, std::ios_base::end);
        std::streampos FileSize = File.tellg();

        std::vector<unsigned char> Bytes(FileSize);

        File.seekg(0, std::ios_base::beg);
        File.read(reinterpret_cast<char*>(Bytes.data()), FileSize);

        this->SarcFile::SarcFile(Bytes);

        File.close();
    }
    else
    {
        std::cerr << "Could not open file \"" << Path << "\"!\n";
    }
}