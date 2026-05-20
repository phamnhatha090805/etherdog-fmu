#pragma once

#include "etherdog.hpp"

void WriteFmuDoubleToPdo(const EtherDOG::Mapping &m, double fmu_value);

void WriteFmuIntToPdo(const EtherDOG::Mapping &m, int32_t fmu_value);

void WriteFmuBoolToPdo(const EtherDOG::Mapping &m, fmi2Boolean fmu_value);

double ReadPdoToFmuDouble(const EtherDOG::Mapping &m);

int32_t ReadPdoToFmuInt(const EtherDOG::Mapping &m);

fmi2Boolean ReadPdoToFmuBool(const EtherDOG::Mapping &m);