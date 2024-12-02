[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/l9Jxgebc)

## Implementation Details:

Performance Characteristics for Efficiency:
- O(1) cache lookup
- O(path_length) Trie insertion/retrieval
- Metadata preservation
- Efficient memory management

## Assumptions:
1. There cannot be two files with the same name and directory relative to two storage servers in the File System.
2. path format should not start with ./
- 

## TODO
- [x] Tokenization on Client End
- [x] Fix Client READ Request to work with modified Send file (with headers)
- [x] Use Modified Send File function for Copy file over network 
- [ ] Implement Copy Directory over Network
    1. dest socket opens channel for receiving
    2. src socket opens channel for communication. 
    3. recursively send all directory info. 
        - modified send_directory_contents to send all the information.
    4. Send a bit to stop communication between the two. 

- [ ] LRU with Tries
- [ ] Backup Integration
- [] REFACTOR