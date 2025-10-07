from argparse import ArgumentParser
from types import SimpleNamespace
import os
from mako.lookup import TemplateLookup
import yaml

def parse_yaml_file(file_path):
    try:
        with open(file_path, 'r') as f: 
            data = yaml.safe_load(f)

        #extract the component name from the path
        path_parts = file_path.split('/')
        component = path_parts[1]  # Always use the component directory name
        if len(path_parts) > 3 and path_parts[1] == 'vc':
            # Special case for VC components: include subdirectory
            component = f"{path_parts[1]}_{path_parts[2]}"
        print(f"Parsing {file_path} → component: {component}")

        return{
            'name': component,
            'templates': data.get('templates', []),
            'channels': data.get('channels', {}),
        }
    except Exception as e:
        print(f"ERROR parsing {file_path}: {e}")
        return None

def main():
    """Main function"""
    parser = ArgumentParser()
    parser.add_argument('--input', action='append', required=True, help='Path to YAML configuration file (can be specified multiple times)')
    parser.add_argument('--output-dir', default='generated')
    args = parser.parse_args()

    #Process each yaml file
    for yaml_file in args.input:
        print(f"Processing: {yaml_file}")

        #PARSE YAML file 
        component_data = parse_yaml_file(yaml_file)
        
        if not component_data['templates']:
            print(f"SKIPPING - No templates defined in {yaml_file}")
            continue
        
        if not component_data['channels']:
            print(f"SKIPPING - No channels defined in {yaml_file}")
        
        print(f"Component: {component_data['name']}")
        print(f"Templates: {len(component_data['templates'])}")
        print(f"Channels: {len(component_data['channels'])}")

        #component specific channels list
        component_channels = []
        for channel_name, channel_config in component_data['channels'].items():
            component_channels.append(SimpleNamespace(
                name = channel_name,
                multiplier = channel_config.get('multiplier', 1.0)
            ))
        
        #component specific output directory
        os.makedirs(args.output_dir, exist_ok=True)
        
        #render each template
        lookup = TemplateLookup(directories=["templates"])

        for template_path in component_data['templates']:
            template_filename = os.path.basename(template_path)
            print(f"Rendering template: {template_filename}")
            template = lookup.get_template(template_filename)
            output = template.render(
                channels = component_channels,
                component = component_data['name']
            ) 

            output_filename = os.path.basename(template_path).replace('.mako','')
            output_file = os.path.join(args.output_dir, output_filename)

            with open(output_file, 'w') as f: 
                f.write(output)

            print(f"Generated: {output_file}")

if __name__ == '__main__':
    main()
