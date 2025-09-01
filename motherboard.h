#pragma once

#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>
#include "projutils.h"

class Motherboard {
private:
	bstr_t description;
	BOOL hosting_board;
	BOOL powered_on;
	bstr_t product;
	bstr_t status;

public:
	Motherboard();
	//TODO - otherconstructor
	virtual ~Motherboard();

	////////////
	// Setters
	////////////
	void setDesc(const bstr_t& desc) { this->description = desc; }
	void setHostingBoard(BOOL val) { this->hosting_board = val; }
	void setPoweredOn(BOOL val) { this->powered_on = val; }
	void setProduct(const bstr_t& prod) { this->product = prod; }
	void setStatus(const bstr_t& stat) { this->status = stat; }

	////////////
	// Getters
	////////////
	const bstr_t& getDesc() const { return this->description; }
	BOOL getHostingBoard() const { return this->hosting_board; }
	BOOL getPoweredOn() const { return this->powered_on; }
	const bstr_t& getProduct() const { return this->product; }
	const bstr_t& getStatus() const { return this->status; }

	// Printing
	void outputMotherboardInfo();
};