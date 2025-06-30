// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef ROBOTICK_ENABLE_MODEL_LOAD_WIP

namespace robotick
{
	struct Model;

	struct ModelLoader
	{
		static bool load_from_yaml(Model& model, const char* yaml_file_path, bool auto_finalize_and_validate = true);
	};

} // namespace robotick

#endif // #ifdef ROBOTICK_ENABLE_MODEL_LOAD_WIP
