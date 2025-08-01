# Dev Notes
A collaborative notes plugin for Unreal 5

### Features
- Backend server decoupled from Unreal Engine
- Persistent notes and note waypoints
- Note tagging
- Note filtering
- Teleport to notes in other levels
- Persistent sessions - you don't have to log in again until the server restarts
- Runtime note creation for bug reports
- Automatic syncing between machines and instances of Unreal
- Auto refresh every 30 seconds

### Setup
Consult the [User Manual](https://docs.google.com/document/d/1RDGf7shMjbeXrR-j34cpeKmhy9rqHVYJ/edit?usp=sharing&ouid=104705768550996225567&rtpof=true&sd=true) for more information.
- Get and set up the [DevNotes Server](https://github.com/Crab-Cafe/DevNotes-Server)
- Install the DevNotes plugin to `YourProject/Plugins/DevNotes`
- Locate DevNotes Settings under Project Settings -> Engine
- Configure server address to match the IP and port of your DevNotes server
- Open the Notes dropdown from the Unreal Toolbar
- Sign in using your DevNotes server credentials

### Filter Syntax
#### Lookup: field=value
`map=TestMap` <br>
`user=DefaultUser` <br>
`tag=Bug`

#### AND: field=value1 value2
`tag=Bug art "level design"` <br>
`user="Runtime Submission" DefaultUser`


#### OR: field=value1 field=value2
`tag=bug tag=art` <br>
`user=DefaultUser user="Runtime Submission" tag=bug`


#### Wildcard: value1 value2
`DefaultUser bug art`
