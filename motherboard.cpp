#include "motherboard.h"	

Motherboard::~Motherboard() {

}

void Motherboard::outputMotherboardInfo() {
    // Output with null checks
    if (BSTR(this->description) != NULL)
        std::wcout << "Description: " << this->description << std::endl;

    if (this->hosting_board != NULL)
        std::wcout << "HostingBoard: " << (this->powered_on == 0 ? "False" : "True") << std::endl;

    if(this->powered_on != NULL)
        std::wcout << "PoweredOn: " << (this->powered_on == 0 ? "False" : "True") << std::endl;

    if (BSTR(this->product) != NULL)
        std::wcout << "Product: " << this->product << std::endl;

    if (BSTR(this->status) != NULL)
        std::wcout << "Status: " << this->status << std::endl;

}