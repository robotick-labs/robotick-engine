#if 0
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <sstream>
#include <unordered_set>

namespace robotick
{

	FieldPathParseError::FieldPathParseError(const std::string& msg) : std::runtime_error(msg)
	{
	}

	bool is_valid_section(const std::string& s)
	{
		return s == "input" || s == "output" || s == "config";
	}

	ParsedFieldPath parse_field_path(const std::string& raw)
	{
		std::istringstream ss(raw);
		std::string token;
		std::vector<std::string> tokens;

		while (std::getline(ss, token, '.'))
		{
			if (token.empty())
			{
				throw FieldPathParseError("Empty segment in field path: " + raw);
			}
			tokens.push_back(token);
		}

		if (tokens.size() < 3)
		{
			throw FieldPathParseError("Expected at least 3 components in field path: " + raw);
		}

		if (!is_valid_section(tokens[1]))
		{
			throw FieldPathParseError("Invalid section '" + tokens[1] + "' in path: " + raw);
		}

		ParsedFieldPath result;
		result.workload_name = tokens[0];
		result.section_name = tokens[1];
		for (size_t i = 2; i < tokens.size(); ++i)
		{
			result.field_path.emplace_back(tokens[i]);
		}

		return result;
	}

	const BlackboardField* find_field_recursive(
		const std::vector<BlackboardField>& schema, const std::vector<FixedString64>& path, size_t& offset_out)
	{
		const std::vector<BlackboardField>* current_schema = &schema;
		size_t current_offset = 0;

		for (size_t i = 0; i < path.size(); ++i)
		{
			bool found = false;

			for (const auto& field : *current_schema)
			{
				if (field.name == path[i])
				{
					current_offset += field.offset;

					if (i == path.size() - 1)
					{
						offset_out = current_offset;
						return &field;
					}

					if (field.type != typeid(Blackboard))
					{
						throw std::runtime_error("Field '" + std::string(field.name.c_str()) + "' is not a blackboard");
					}

					current_schema = &field.subfields;
					found = true;
					break;
				}
			}

			if (!found)
			{
				throw std::runtime_error("Field '" + std::string(path[i].c_str()) + "' not found");
			}
		}

		throw std::runtime_error("Field path resolution failed");
	}

	const std::vector<BlackboardField>& get_schema_section(const WorkloadInstanceInfo& info, const FixedString64& section)
	{
		if (section == "input")
			return info.input_schema;
		if (section == "output")
			return info.output_schema;
		if (section == "config")
			return info.config_schema;
		throw std::runtime_error("Invalid blackboard section: " + std::string(section.c_str()));
	}

	uint8_t* resolve_ptr(
		const ParsedFieldPath& path, const BlackboardsBuffer& blackboards, bool is_write, std::type_index& type_out, size_t& size_out)
	{
		const WorkloadInstanceInfo& workload_info = WorkloadRegistry::get(path.workload_name);
		const std::vector<BlackboardField>& schema = get_schema_section(workload_info, path.section_name);

		size_t offset = 0;
		const BlackboardField* field = find_field_recursive(schema, path.field_path, offset);

		type_out = field->type;
		size_out = field->size;

		const uint8_t* base = blackboards.get_source().raw_ptr();
		return const_cast<uint8_t*>(base + field->offset);
	}

	std::vector<DataConnectionInfo> DataConnectionResolver::resolve(
		const std::vector<DataConnectionSeed>& seeds, const BlackboardsBuffer& blackboards)
	{
		std::vector<DataConnectionInfo> results;
		std::unordered_set<std::string> seen_destinations;

		for (const auto& seed : seeds)
		{
			ParsedFieldPath src = parse_field_path(seed.source_path);
			ParsedFieldPath dst = parse_field_path(seed.dest_path);

			std::type_index src_type, dst_type;
			size_t src_size = 0, dst_size = 0;

			const void* source_ptr = resolve_ptr(src, blackboards, false, src_type, src_size);
			void* dest_ptr = resolve_ptr(dst, blackboards, true, dst_type, dst_size);

			if (src_type != dst_type || src_size != dst_size)
			{
				throw std::runtime_error("Type or size mismatch between '" + seed.source_path + "' and '" + seed.dest_path + "'");
			}

			std::string dest_key = dst.workload_name.c_str() + "." + dst.section_name.c_str() + "." + dst.field_path.front().c_str();
			if (!seen_destinations.insert(dest_key).second)
			{
				throw std::runtime_error("Duplicate destination field: " + dest_key);
			}

			results.push_back(DataConnectionInfo{source_ptr, dest_ptr, src_size, src_type});
		}

		return results;
	}

} // namespace robotick
#endif // if 0