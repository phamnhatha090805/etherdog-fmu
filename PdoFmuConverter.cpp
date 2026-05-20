#include "PdoFmuConverter.hpp"

template <typename Target, typename Source>
static void WriteAs(void *destination, Source value)
{
    Target converted = static_cast<Target>(value);
    std::memcpy(destination, &converted, sizeof(Target));
}

template <typename Source, typename Target>
static Target ReadAs(const void *source)
{
    Source value;
    std::memcpy(&value, source, sizeof(Source));
    return static_cast<Target>(value);
}

void WriteFmuDoubleToPdo(const EtherDOG::Mapping &m, double fmu_value)
{
    // This function is used to write a double value from the FMU variable to the PDO memory based on the mapping. It handles the necessary type conversion and byte copying based on the data type of the CoE entry.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        WriteAs<double>(m.entry.data, fmu_value); // Write double value directly
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        WriteAs<float>(m.entry.data, fmu_value); // Convert double to float and write
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        WriteAs<bool>(m.entry.data, fmu_value != 0); // Convert int32_t to bool (non-zero is true)
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        WriteAs<int8_t>(m.entry.data, fmu_value); // Convert double to int8_t
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        WriteAs<uint8_t>(m.entry.data, fmu_value); // Convert double to uint8_t
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        WriteAs<int16_t>(m.entry.data, fmu_value); // Convert double to int16_t
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        WriteAs<uint16_t>(m.entry.data, fmu_value); // Convert double to uint16_t
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        WriteAs<int32_t>(m.entry.data, fmu_value); // Convert double to int32_t
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        WriteAs<uint32_t>(m.entry.data, fmu_value); // Convert double to uint32_t
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

void WriteFmuIntToPdo(const EtherDOG::Mapping &m, int32_t fmu_value)
{
    // This function can be used to write an integer value from the FMU variable to the PDO memory based on the mapping. It should handle the necessary type conversion and byte copying based on the data type of the CoE entry.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        WriteAs<double>(m.entry.data, fmu_value); // Convert int32_t to double
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        WriteAs<float>(m.entry.data, fmu_value); // Convert int32_t to float
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        WriteAs<bool>(m.entry.data, fmu_value != 0); // Convert int32_t to bool (non-zero is true)
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        WriteAs<int8_t>(m.entry.data, fmu_value); // Convert int32_t to int8_t
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        WriteAs<uint8_t>(m.entry.data, fmu_value); // Convert int32_t to uint8_t
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        WriteAs<int16_t>(m.entry.data, fmu_value); // Convert int32_t to int16_t
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        WriteAs<uint16_t>(m.entry.data, fmu_value); // Convert int32_t to uint16_t
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        WriteAs<int32_t>(m.entry.data, fmu_value); // Assuming fmu_value is already an int32_t
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        WriteAs<uint32_t>(m.entry.data, fmu_value); // Convert int32_t to uint32_t
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

void WriteFmuBoolToPdo(const EtherDOG::Mapping &m, fmi2Boolean fmu_value)
{
    // This function can be used to write a boolean value from the FMU variable to the PDO memory based on the mapping. It should handle the necessary type conversion and byte copying based on the data type of the CoE entry.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value_to_write = fmu_value ? 1.0 : 0.0; // Convert bool to double (true=1.0, false=0.0)
        WriteAs<double>(m.entry.data, value_to_write);
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value_to_write = fmu_value ? 1.0f : 0.0f; // Convert bool to float (true=1.0f, false=0.0f)
        WriteAs<float>(m.entry.data, value_to_write);
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value_to_write = fmu_value; // Assuming fmu_value is already a fmi2Boolean
        WriteAs<bool>(m.entry.data, value_to_write);
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value_to_write = fmu_value ? 1 : 0; // Convert bool to int8_t (true=1, false=0)
        WriteAs<int8_t>(m.entry.data, value_to_write);
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value_to_write = fmu_value ? 1 : 0; // Convert bool to uint8_t (true=1, false=0)
        WriteAs<uint8_t>(m.entry.data, value_to_write);
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        int16_t value_to_write = fmu_value ? 1 : 0; // Convert bool to int16_t (true=1, false=0)
        WriteAs<int16_t>(m.entry.data, value_to_write);
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value_to_write = fmu_value ? 1 : 0; // Convert bool to uint16_t (true=1, false=0)
        WriteAs<uint16_t>(m.entry.data, value_to_write);
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value_to_write = fmu_value ? 1 : 0; // Convert bool to int32_t (true=1, false=0)
        WriteAs<int32_t>(m.entry.data, value_to_write);
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value_to_write = fmu_value ? 1 : 0; // Convert bool to uint32_t (true=1, false=0)
        WriteAs<uint32_t>(m.entry.data, value_to_write);
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

double ReadPdoToFmuDouble(const EtherDOG::Mapping &m)
{
    // This function reads the value from the PDO memory based on the mapping and converts it to double to be written to the FMU variable.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        return ReadAs<double, double>(m.entry.data); // Read double value directly
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        return ReadAs<float, double>(m.entry.data); // Read float value and convert to double
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        return ReadAs<bool, double>(m.entry.data); // Read bool value and convert to double (true=1.0, false=0.0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        return ReadAs<int8_t, double>(m.entry.data); // Read int8_t value and convert to double
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        return ReadAs<uint8_t, double>(m.entry.data); // Read uint8_t value and convert to double
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        return ReadAs<int16_t, double>(m.entry.data); // Read int16_t value and convert to double
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        return ReadAs<uint16_t, double>(m.entry.data); // Read uint16_t value and convert to double
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        return ReadAs<int32_t, double>(m.entry.data); // Read int32_t value and convert to double
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        return ReadAs<uint32_t, double>(m.entry.data); // Read uint32_t value and convert to double
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0.0;
    }
    }
}

int32_t ReadPdoToFmuInt(const EtherDOG::Mapping &m)
{
    // This function can be used to read the value from the PDO memory based on the mapping and convert it to int32_t to be written to the FMU variable.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        return ReadAs<double, int32_t>(m.entry.data); // Read double value and convert to int32_t
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        return ReadAs<float, int32_t>(m.entry.data); // Read float value and convert to int32_t
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        return ReadAs<bool, int32_t>(m.entry.data); // Read bool value and convert to int32_t (true=1, false=0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        return ReadAs<int8_t, int32_t>(m.entry.data); // Read int8_t value and convert to int32_t
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        return ReadAs<uint8_t, int32_t>(m.entry.data); // Read uint8_t value and convert to int32_t
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        return ReadAs<int16_t, int32_t>(m.entry.data); // Read int16_t value and convert to int32_t
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        return ReadAs<uint16_t, int32_t>(m.entry.data); // Read uint16_t value and convert to int32_t
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        return ReadAs<int32_t, int32_t>(m.entry.data); // Read int32_t value and convert to int32_t
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        return ReadAs<uint32_t, int32_t>(m.entry.data); // Read uint32_t value and convert to int32_t
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0; // Default to 0 if unsupported type
    }
    }
}

fmi2Boolean ReadPdoToFmuBool(const EtherDOG::Mapping &m)
{
    // This function can be used to read the value from the PDO memory based on the mapping and convert it to fmi2Boolean to be written to the FMU variable.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        return ReadAs<double, fmi2Boolean>(m.entry.data); // Read double value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        return ReadAs<float, fmi2Boolean>(m.entry.data); // Read float value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        return ReadAs<bool, fmi2Boolean>(m.entry.data); // Read bool value and convert to fmi2Boolean (true=1, false=0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        return ReadAs<int8_t, fmi2Boolean>(m.entry.data); // Read int8_t value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        return ReadAs<uint8_t, fmi2Boolean>(m.entry.data); // Read uint8_t value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        return ReadAs<int16_t, fmi2Boolean>(m.entry.data); // Read int16_t value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        return ReadAs<uint16_t, fmi2Boolean>(m.entry.data); // Read uint16_t value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        return ReadAs<int32_t, fmi2Boolean>(m.entry.data); // Read int32_t value and convert to fmi2Boolean (non-zero is true)
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        return ReadAs<uint32_t, fmi2Boolean>(m.entry.data); // Read uint32_t value and convert to fmi2Boolean (non-zero is true)
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0; // Default to false if unsupported type
    }
    }
}