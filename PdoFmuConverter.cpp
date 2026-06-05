#include "PdoFmuConverter.hpp"

template <typename Target, typename Source>
static void WriteAs(void *destination, Source value)
{
    Target converted = static_cast<Target>(value);
    std::memcpy(destination, &converted, sizeof(Target));
}

template <typename Source>
static void WriteFmuToPdo(const EtherDOG::Mapping &m, Source value)
{
    using namespace kickcat::CoE;

    switch (m.entry->type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        WriteAs<double>(m.entry->data, value); // Convert Source to double and write
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        WriteAs<float>(m.entry->data, value); // Convert double to float and write
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        WriteAs<bool>(m.entry->data, value != 0); // Convert Source to bool (non-zero is true)
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        WriteAs<int8_t>(m.entry->data, value); // Convert Source to int8_t
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        WriteAs<uint8_t>(m.entry->data, value); // Convert Source to uint8_t
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        WriteAs<int16_t>(m.entry->data, value); // Convert Source to int16_t
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        WriteAs<uint16_t>(m.entry->data, value); // Convert Source to uint16_t
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        WriteAs<int32_t>(m.entry->data, value); // Convert Source to int32_t
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        WriteAs<uint32_t>(m.entry->data, value); // Convert Source to uint32_t
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry->type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

template <typename Source, typename Target>
static Target ReadAs(const void *source)
{
    Source value;
    std::memcpy(&value, source, sizeof(Source));
    return static_cast<Target>(value);
}

template <typename Target>
static Target ReadPdoAs(const EtherDOG::Mapping &m)
{
    using namespace kickcat::CoE;

    switch (m.entry->type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        return ReadAs<double, Target>(m.entry->data); // Read double value and convert to Target
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        return ReadAs<float, Target>(m.entry->data); // Read float value and convert to Target
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        return ReadAs<bool, Target>(m.entry->data); // Read bool value and convert to Target (true=1, false=0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        return ReadAs<int8_t, Target>(m.entry->data); // Read int8_t value and convert to Target
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        return ReadAs<uint8_t, Target>(m.entry->data); // Read uint8_t value and convert to Target
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        return ReadAs<int16_t, Target>(m.entry->data); // Read int16_t value and convert to Target
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        return ReadAs<uint16_t, Target>(m.entry->data); // Read uint16_t value and convert to Target
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        return ReadAs<int32_t, Target>(m.entry->data); // Read int32_t value and convert to Target
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        return ReadAs<uint32_t, Target>(m.entry->data); // Read uint32_t value and convert to Target
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry->type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0; // Default to 0 if unsupported type
    }
    }
}

void WriteFmuDoubleToPdo(const EtherDOG::Mapping &m, double fmu_value)
{
    WriteFmuToPdo(m, fmu_value);
}

void WriteFmuIntToPdo(const EtherDOG::Mapping &m, int32_t fmu_value)
{
    WriteFmuToPdo(m, fmu_value);
}

void WriteFmuBoolToPdo(const EtherDOG::Mapping &m, fmi2Boolean fmu_value)
{
    WriteFmuToPdo(m, fmu_value);
}

double ReadPdoToFmuDouble(const EtherDOG::Mapping &m)
{
    return ReadPdoAs<double>(m);
}

int32_t ReadPdoToFmuInt(const EtherDOG::Mapping &m)
{
    return ReadPdoAs<int32_t>(m);
}

fmi2Boolean ReadPdoToFmuBool(const EtherDOG::Mapping &m)
{
    return ReadPdoAs<fmi2Boolean>(m);
}