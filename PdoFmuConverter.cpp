#include "PdoFmuConverter.hpp"

// Little-endian read/write of a bit field in the process image. Handles sub-byte
// fields (an EL1004 packs four 1-bit BOOLs in one byte) and any alignment up to
// 64 bits. `pi` is the buffer passed to PDO::setInput / PDO::setOutput.
void readPiBits(uint8_t const *pi, uint32_t bit_offset, uint16_t bit_len, uint8_t *buffer)
{
    for (uint16_t i = 0; i < bit_len; ++i)
    {
        uint32_t pi_bit = bit_offset + i;
        uint64_t b = (pi[pi_bit / 8] >> (pi_bit % 8)) & 0x1u;
        buffer[i / 8] |= (b << (i % 8));
    }
}

void writePiBits(uint8_t *pi, uint32_t bit_offset, uint16_t bit_len, const uint8_t *buffer)
{
    for (uint16_t i = 0; i < bit_len; ++i)
    {
        uint32_t pi_bit = bit_offset + i;
        uint8_t mask = static_cast<uint8_t>(1u << (pi_bit % 8));
        // TODO: check endianness and bit ordering
        uint64_t b = (buffer[i / 8] >> (i % 8)) & 0x1u;
        if (b == 0)
        {
            pi[pi_bit / 8] &= static_cast<uint8_t>(~mask);
        }
        else
        {
            pi[pi_bit / 8] |= mask;
        }
    }
}

template <typename Target, typename Source> static void WriteAs(const EtherDOG::Mapping &m, Source value)
{

    // step 1: convert:
    Target converted = static_cast<Target>(value);

    // Step 2: convert to bytes:
    uint8_t buffer[8]; // temp buffer.
    static_assert(sizeof(Target) <= 8, "Target type is too large for buffer");
    std::memcpy(buffer, &converted, sizeof(Target));

    // Step 3: write to pi:
    assert(m.BitLen <= 64);
    assert(m.input_process_image != nullptr);
    writePiBits(m.input_process_image, m.PiBitOffset, m.BitLen, buffer);
}

template <typename Source> static void WriteFmuToPdo(const EtherDOG::Mapping &m, Source value)
{
    using namespace kickcat::CoE;

    switch (m.entry->type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        WriteAs<double>(m, value); // Convert Source to double and write
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        WriteAs<float>(m, value); // Convert double to float and write
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        WriteAs<bool>(m, value != 0); // Convert Source to bool (non-zero is true)
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        WriteAs<int8_t>(m, value); // Convert Source to int8_t
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        WriteAs<uint8_t>(m, value); // Convert Source to uint8_t
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        WriteAs<int16_t>(m, value); // Convert Source to int16_t
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        WriteAs<uint16_t>(m, value); // Convert Source to uint16_t
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        WriteAs<int32_t>(m, value); // Convert Source to int32_t
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        WriteAs<uint32_t>(m, value); // Convert Source to uint32_t
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry->type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

template <typename Source, typename Target> static Target ReadAs(const EtherDOG::Mapping &m)
{
    assert(m.BitLen <= 64); // Ensure we don't read more than 64 bits

    // Step 1: Read the raw bits from the process image into a buffer
    uint8_t buffer[8];                 // temp buffer.
    memset(buffer, 0, sizeof(buffer)); // Clear the buffer before reading
    assert(m.output_process_image != nullptr);
    readPiBits(m.output_process_image, m.PiBitOffset, m.BitLen, buffer);

    // Step 2: Convert the buffer to the source type
    Source value;
    static_assert(sizeof(Source) <= 8, "Source type is too large for buffer");
    std::memcpy(&value, buffer, sizeof(Source));

    // step 3: Cast to the target type and return
    return static_cast<Target>(value);
}

template <typename Target> static Target ReadPdoAs(const EtherDOG::Mapping &m)
{
    using namespace kickcat::CoE;

    switch (m.entry->type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        return ReadAs<double, Target>(m); // Read double value and convert to Target
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        return ReadAs<float, Target>(m); // Read float value and convert to Target
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        return ReadAs<bool, Target>(m); // Read bool value and convert to Target (true=1, false=0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        return ReadAs<int8_t, Target>(m); // Read int8_t value and convert to Target
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        return ReadAs<uint8_t, Target>(m); // Read uint8_t value and convert to Target
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        return ReadAs<int16_t, Target>(m); // Read int16_t value and convert to Target
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        return ReadAs<uint16_t, Target>(m); // Read uint16_t value and convert to Target
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        return ReadAs<int32_t, Target>(m); // Read int32_t value and convert to Target
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        return ReadAs<uint32_t, Target>(m); // Read uint32_t value and convert to Target
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