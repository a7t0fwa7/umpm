# umpm
A demonstration highlighting how page tables can be modified from user mode to enable on-demand mapping, providing access to arbitrary physical memory.

## Building

1. Clone the repository:
   ```
   git clone https://github.com/Hxnter999/umpm.git
   cd umpm
   ```

2. Create and enter build directory:
   ```
   mkdir build && cd build
   ```

3. Configure and build:
   ```
   cmake ..
   cmake --build . -- config Release
   ```

## How it works
- The project is straightforward, it involves modifying page tables to achieve specific mappings. 
- **You need a way of modifying your page tables** initially in whatever way you like. In this case, I'm using a simple kernel driver. 
- Allocate a 4kb buffer in usermode and ask the kernel to modify its mapping to point to the pt itself. Creating a **self referencing pte**.
- Through this pte **the usermode process can access the pt** and modify it's entries to map in any part of physical memory.
- This ensures that everything besides the initial page table modification, is done in usermode and **doesn't require any kernel interaction**.
- For cleanup, we can **simply restore the pfn of the pte we initially modified** to be a self referencing entry, this is also **possible from usermode**. The reason it must be restored is because of the windows vmm which will cause a bigcheck in the case of our illegal mappings.

This is how the page tables look after adding the self referencing pte:
![PTView](assets/ptview.png)
*The application you see in the screenshot is [PTView](https://github.com/VollRagm/PTView)*
